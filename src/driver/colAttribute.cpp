#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <string>

#include "../util/valuePtrHelper.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"
#include "mappings/typeMappings.hpp"

/*
Whether values of an ODBC type are character strings. Those are the
types that compare case-sensitively, work with LIKE, and are written
as literals between single quotes.
*/
static bool isCharacterType(SQLSMALLINT odbcTypeCode) {
  return odbcTypeCode == SQL_CHAR or odbcTypeCode == SQL_VARCHAR or
         odbcTypeCode == SQL_LONGVARCHAR or odbcTypeCode == SQL_WCHAR or
         odbcTypeCode == SQL_WVARCHAR or odbcTypeCode == SQL_WLONGVARCHAR;
}

/*
The most characters it takes to show a value of this column, as ODBC
defines it for SQL_DESC_DISPLAY_SIZE. SQL_NO_TOTAL when it isn't known,
such as for a varchar with no length.
*/
static SQLLEN getDisplaySize(SQLSMALLINT odbcTypeCode,
                             DescriptorField& columnInfo) {
  switch (odbcTypeCode) {
    case SQL_BIT: {
      return 1;
    }
    case SQL_TINYINT: {
      return 4;
    }
    case SQL_SMALLINT: {
      return 6;
    }
    case SQL_INTEGER: {
      return 11;
    }
    case SQL_BIGINT: {
      return 20;
    }
    case SQL_REAL: {
      return 14;
    }
    case SQL_FLOAT:
    case SQL_DOUBLE: {
      return 24;
    }
    case SQL_DECIMAL:
    case SQL_NUMERIC: {
      // Room for a sign and a decimal point.
      return columnInfo.precision + 2;
    }
    case SQL_TYPE_DATE: {
      // yyyy-mm-dd
      return 10;
    }
    case SQL_TYPE_TIME: {
      // hh:mm:ss, then a point and the fractional seconds.
      return columnInfo.scale > 0 ? 9 + columnInfo.scale : 8;
    }
    case SQL_TYPE_TIMESTAMP: {
      // yyyy-mm-dd hh:mm:ss, then a point and the fractional seconds.
      return columnInfo.scale > 0 ? 20 + columnInfo.scale : 19;
    }
    case SQL_GUID: {
      return 36;
    }
    default: {
      if (isCharacterType(odbcTypeCode) and columnInfo.length > 0) {
        return columnInfo.length;
      }
      return SQL_NO_TOTAL;
    }
  }
}

/*
The work of SQLColAttribute and SQLColAttributeW. String attributes
are written for the kind of function that was called, and
BufferLength is in bytes for both.

Numeric attributes must be written as a whole SQLLEN. Writing only 4
bytes on 64-bit leaves the upper half untouched, so a negative type
code such as SQL_BIGINT (-5) or SQL_BIT (-7) is read back by the
application as a large positive number.
*/
static SQLRETURN colAttribute(SQLHSTMT StatementHandle,
                              SQLUSMALLINT ColumnNumber,
                              SQLUSMALLINT FieldIdentifier,
                              SQLPOINTER CharacterAttributePtr,
                              SQLSMALLINT BufferLength,
                              SQLSMALLINT* StringLengthPtr,
                              SQLPOINTER NumericAttributePtr,
                              TextEncoding encoding) {
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
      SQLSMALLINT odbcTypeCode = columnInfo.odbcDataType;
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = odbcTypeCode;
      }
      break;
    }
    case SQL_COLUMN_LENGTH: { // 3
      // The ODBC 2 length, which is the number of bytes transferred.
      // ODBC 3 applications ask for SQL_DESC_OCTET_LENGTH instead.
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) =
            columnInfo.octetLength;
      }
      break;
    }
    case SQL_COLUMN_PRECISION: { // 4
      // The ODBC 2 version of SQL_DESC_PRECISION.
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = columnInfo.precision;
      }
      break;
    }
    case SQL_COLUMN_SCALE: { // 5
      // The ODBC 2 version of SQL_DESC_SCALE.
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = columnInfo.scale;
      }
      break;
    }
    case SQL_DESC_DISPLAY_SIZE: { // 6
      SQLSMALLINT odbcTypeCode = columnInfo.odbcDataType;
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) =
            getDisplaySize(odbcTypeCode, columnInfo);
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
    case SQL_DESC_FIXED_PREC_SCALE: { // 9
      // True only for money-like types, which Trino doesn't have.
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = SQL_FALSE;
      }
      break;
    }
    case SQL_DESC_UPDATABLE: { // 10
      // The driver is read-only.
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = SQL_ATTR_READONLY;
      }
      break;
    }
    case SQL_DESC_AUTO_UNIQUE_VALUE: { // 11
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = SQL_FALSE;
      }
      break;
    }
    case SQL_DESC_CASE_SENSITIVE: { // 12
      // Trino compares strings case-sensitively.
      SQLSMALLINT odbcTypeCode = columnInfo.odbcDataType;
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) =
            isCharacterType(odbcTypeCode) ? SQL_TRUE : SQL_FALSE;
      }
      break;
    }
    case SQL_DESC_SEARCHABLE: { // 13
      // Every column can be used in a WHERE clause, and strings can
      // be used with LIKE as well.
      SQLSMALLINT odbcTypeCode = columnInfo.odbcDataType;
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) =
            isCharacterType(odbcTypeCode) ? SQL_PRED_SEARCHABLE
                                          : SQL_PRED_BASIC;
      }
      break;
    }
    case SQL_COLUMN_TYPE_NAME: { // 14
      WriteLog(LL_TRACE, "  Getting SQL column name");
      std::string columnTypeName = columnInfo.trinoRawTypeName;
      truncated = writeNullTermStringToPtr(CharacterAttributePtr,
                                           columnTypeName,
                                           BufferLength,
                                           StringLengthPtr,
                                           encoding);
      break;
    }
    case SQL_DESC_TABLE_NAME:        // 15
    case SQL_DESC_SCHEMA_NAME:       // 16
    case SQL_DESC_CATALOG_NAME:      // 17
    case SQL_DESC_BASE_TABLE_NAME: { // 23
      // Trino doesn't say which table a result column came from, and
      // ODBC uses an empty string for that.
      truncated = writeNullTermStringToPtr(
          CharacterAttributePtr, "", BufferLength, StringLengthPtr, encoding);
      break;
    }
    case SQL_DESC_LABEL:              // 18
    case SQL_DESC_BASE_COLUMN_NAME: { // 22
      // Trino gives each result column one name, which serves as its
      // label and its base column name.
      truncated = writeNullTermStringToPtr(CharacterAttributePtr,
                                           columnInfo.columnName,
                                           BufferLength,
                                           StringLengthPtr,
                                           encoding);
      break;
    }
    case SQL_DESC_LITERAL_PREFIX:   // 27
    case SQL_DESC_LITERAL_SUFFIX: { // 28
      // String literals are quoted. Other literals need nothing
      // ODBC can express as a fixed prefix and suffix.
      SQLSMALLINT odbcTypeCode = columnInfo.odbcDataType;
      std::string quote        = isCharacterType(odbcTypeCode) ? "'" : "";
      truncated                = writeNullTermStringToPtr(CharacterAttributePtr,
                                           quote,
                                           BufferLength,
                                           StringLengthPtr,
                                           encoding);
      break;
    }
    case SQL_DESC_LOCAL_TYPE_NAME: { // 29
      // Trino's type names aren't localized.
      truncated = writeNullTermStringToPtr(CharacterAttributePtr,
                                           columnInfo.trinoRawTypeName,
                                           BufferLength,
                                           StringLengthPtr,
                                           encoding);
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
    case SQL_DESC_ROWVER: { // 35
      // No column is updated automatically when a row changes.
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = SQL_FALSE;
      }
      break;
    }
    case SQL_DESC_COUNT: { // 1001
      // The number of result columns, as SQLNumResultCols reports it.
      // The IRD can't be used, because it has a slot for column 0, the
      // bookmark column. ColumnNumber is ignored.
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) =
            statement->trinoQuery->getColumnCount();
      }
      break;
    }
    case SQL_DESC_TYPE: { // 1002
      // The same as the concise type, except that dates and times
      // are reported as SQL_DATETIME.
      SQLSMALLINT odbcTypeCode = columnInfo.odbcDataType;
      if (odbcTypeCode == SQL_TYPE_DATE or odbcTypeCode == SQL_TYPE_TIME or
          odbcTypeCode == SQL_TYPE_TIMESTAMP) {
        odbcTypeCode = SQL_DATETIME;
      }
      if (NumericAttributePtr) {
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) = odbcTypeCode;
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
      truncated              = writeNullTermStringToPtr(CharacterAttributePtr,
                                           columnName,
                                           BufferLength,
                                           StringLengthPtr,
                                           encoding);
      break;
    }
    case SQL_DESC_UNNAMED: { // 1012
      WriteLog(LL_TRACE, "  Getting SQL column named-ness");
      if (NumericAttributePtr) {
        // SQL_NAMED is 0 and SQL_UNNAMED is 1, the opposite of what
        // the named flag holds, so it can't be written as it is.
        *reinterpret_cast<SQLLEN*>(NumericAttributePtr) =
            columnInfo.named ? SQL_NAMED : SQL_UNNAMED;
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

/*
 The signatures of these functions differ between 64-bit and 32-bit
 targets. For 32-bit, NumericAttributePtr is a SQLPOINTER, but for
 64-bit, its a SQLLEN*.
 */
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
  WriteLog(LL_TRACE, "Entering SQLColAttribute");
  return colAttribute(StatementHandle,
                      ColumnNumber,
                      FieldIdentifier,
                      CharacterAttributePtr,
                      BufferLength,
                      StringLengthPtr,
                      NumericAttributePtr,
                      AnsiText);
}

#if defined(_WIN64)
SQLRETURN SQL_API SQLColAttributeW(SQLHSTMT StatementHandle,
                                   SQLUSMALLINT ColumnNumber,
                                   SQLUSMALLINT FieldIdentifier,
                                   _Out_writes_bytes_opt_(BufferLength)
                                       SQLPOINTER CharacterAttributePtr,
                                   SQLSMALLINT BufferLength,
                                   _Out_opt_ SQLSMALLINT* StringLengthPtr,
                                   _Out_opt_ SQLLEN* NumericAttributePtr) {
#else
SQLRETURN SQL_API SQLColAttributeW(SQLHSTMT StatementHandle,
                                   SQLUSMALLINT ColumnNumber,
                                   SQLUSMALLINT FieldIdentifier,
                                   _Out_writes_bytes_opt_(BufferLength)
                                       SQLPOINTER CharacterAttributePtr,
                                   SQLSMALLINT BufferLength,
                                   _Out_opt_ SQLSMALLINT* StringLengthPtr,
                                   _Out_opt_ SQLPOINTER NumericAttributePtr) {
#endif
  WriteLog(LL_TRACE, "Entering SQLColAttributeW");
  return colAttribute(StatementHandle,
                      ColumnNumber,
                      FieldIdentifier,
                      CharacterAttributePtr,
                      BufferLength,
                      StringLengthPtr,
                      NumericAttributePtr,
                      UnicodeText);
}
#pragma warning(pop)
