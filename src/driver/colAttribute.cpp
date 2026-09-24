#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <string>

#include "../util/valuePtrHelper.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"
#include "mappings/typeMappings.hpp"

#pragma warning(push)
/*
The CharacterAttributePtr and NumericAttributePtr fields may
be unused if they aren't relevant to the requested attribute/column.
This is a documented expectation for these out parameters, so we'll
disable the warning about returning uninitialized memory in an out
parameter.
https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolattribute-function
*/
#pragma warning(disable : 6101)

/*
 The signature of this function differs between 64-bit and 32-bit
 targets. For 32-bit, NumericAttributePtr is a SQLPOINTER, but for
 64-bit, its a SQLLEN*.

 Either way, numeric attributes must be written as a whole SQLLEN.
 Writing only 4 bytes on 64-bit leaves the upper half untouched, so a
 negative type code such as SQL_BIGINT (-5) or SQL_BIT (-7) is read
 back by the application as a large positive number.
 */
#if defined(_WIN64)
SQLRETURN SQL_API SQLColAttribute(SQLHSTMT StatementHandle,
                                  SQLUSMALLINT ColumnNumber,
                                  SQLUSMALLINT FieldIdentifier,
                                  _Out_writes_bytes_opt_(BufferLength)
                                      SQLPOINTER CharacterAttributePtr,
                                  SQLSMALLINT BufferLength,
                                  _Out_opt_ SQLSMALLINT* StringLengthPtr,
                                  _Out_opt_ SQLLEN* NumericAttributePtr) {
#else
SQLRETURN SQL_API SQLColAttribute(SQLHSTMT StatementHandle,
                                  SQLUSMALLINT ColumnNumber,
                                  SQLUSMALLINT FieldIdentifier,
                                  _Out_writes_bytes_opt_(BufferLength)
                                      SQLPOINTER CharacterAttributePtr,
                                  SQLSMALLINT BufferLength,
                                  _Out_opt_ SQLSMALLINT* StringLengthPtr,
                                  _Out_opt_ SQLPOINTER NumericAttributePtr) {
#endif
#pragma warning(pop)
  WriteLog(LL_TRACE, "Entering SQLColAttribute");
  Statement* statement = reinterpret_cast<Statement*>(StatementHandle);
  // A new call starts with no diagnostics from the previous one.
  statement->clearError();

  Descriptor* ird            = statement->impRowDesc;
  DescriptorField columnInfo = ird->getField(ColumnNumber);

  // Set by the cases that return a string, if the caller's buffer
  // was too small to hold all of it. Handled once after the switch.
  bool truncated = false;

  switch (FieldIdentifier) {
    case SQL_DESC_CONCISE_TYPE: { // 2
      WriteLog(LL_TRACE, "  Getting SQL column type");
      SQLSMALLINT odbcTypeCode =
          TRINO_RAW_TYPE_TO_ODBC_TYPE_CODE[columnInfo.trinoRawTypeName];
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = odbcTypeCode;
      }
      break;
    }
    case SQL_DESC_UNSIGNED: { // 8
      // Trino does not support unsigned integer types, they're
      // always mapped to the next larger type. In the case of
      // unsigned int64s, they are mapped to decimals.
      WriteLog(LL_TRACE, "  Getting SQL column signed-nessness");
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = columnInfo.isUnsigned;
      }
      break;
    }
    case SQL_COLUMN_TYPE_NAME: { // 14
      WriteLog(LL_TRACE, "  Getting SQL column name");
      std::string columnTypeName = columnInfo.trinoRawTypeName;
      truncated                  = writeNullTermStringToPtr(
          CharacterAttributePtr, columnTypeName, BufferLength, StringLengthPtr);
      break;
    }
    case SQL_DESC_NUM_PREC_RADIX: { // 32
      WriteLog(LL_TRACE, "  Getting SQL column attribute NumPrecRadix");
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) =
            columnInfo.numPrecRadix;
      }
      break;
    }
    case SQL_DESC_LENGTH: { // 1003
      WriteLog(LL_TRACE, "  Getting SQL column length");
      // How do we handle non-fixed length varchars?
      // For now, this defaults to SQL_NO_TOTAL (-4).
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = columnInfo.length;
      }
      break;
    }
    case SQL_DESC_PRECISION: { // 1005
      WriteLog(LL_TRACE, "  Getting SQL column precision");
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = columnInfo.precision;
      }
      break;
    }
    case SQL_DESC_SCALE: { // 1006
      WriteLog(LL_TRACE, "  Getting SQL column scale");
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = columnInfo.scale;
      }
      break;
    }
    case SQL_DESC_NULLABLE: { // 1008
      WriteLog(LL_TRACE, "  Getting SQL column null-abilty");
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = columnInfo.nullable;
      }
      break;
    }
    case SQL_DESC_NAME: { // 1011
      WriteLog(LL_TRACE, "  Getting SQL column name");
      std::string columnName = columnInfo.columnName;
      truncated              = writeNullTermStringToPtr(
          CharacterAttributePtr, columnName, BufferLength, StringLengthPtr);
      break;
    }
    case SQL_DESC_UNNAMED: { // 1012
      WriteLog(LL_TRACE, "  Getting SQL column named-ness");
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = columnInfo.named;
      }
      break;
    }
    case SQL_DESC_OCTET_LENGTH: { // 1013
      WriteLog(LL_TRACE, "  Getting SQL column octet length");
      SQLULEN octetLength =
          TRINO_RAW_TYPE_TO_ODBC_SIZE_BYTES[columnInfo.trinoRawTypeName];
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) =
            static_cast<SQLLEN>(columnInfo.octetLength);
      }
      break;
    }
    default: {
      WriteLog(LL_ERROR,
               "  ERROR: Unhandled Column attribute type: " +
                   std::to_string(FieldIdentifier));
      return SQL_ERROR;
    }
  }

  if (truncated) {
    WriteLog(LL_WARN,
             "  Exiting SQLColAttribute - the buffer provided for field " +
                 std::to_string(FieldIdentifier) + " was too small");
    // 01004 = String data, right truncated. StringLengthPtr holds the
    // length the application needs to allocate to get the whole value.
    statement->setError(ErrorInfo("String data, right truncated", "01004"));
    return SQL_SUCCESS_WITH_INFO;
  }

  return SQL_SUCCESS;
}
