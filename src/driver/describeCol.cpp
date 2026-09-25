#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <map>

#include "../util/valuePtrHelper.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"
#include "mappings/typeMappings.hpp"

SQLULEN static inferODBCColumnSize(ColumnDescription description) {
  std::string rawType = description.getRawType();
  if (TRINO_RAW_TYPE_TO_ODBC_SIZE_BYTES.count(rawType)) {
    SQLULEN odbcSizeBytes = TRINO_RAW_TYPE_TO_ODBC_SIZE_BYTES[rawType];
    WriteLog(LL_TRACE,
             "  ODBC Type Size inferred as: " + std::to_string(odbcSizeBytes));
    return odbcSizeBytes;
  }
  if (rawType == "varchar") {
    // Varchars have an argument that specifies their length.
    return description.getTypeArguments()[0]["value"];
  }
  throw std::invalid_argument("Cannot determine size of column: " +
                              description.getName());
}

SQLSMALLINT static inferODBCTypeCode(ColumnDescription description) {
  std::string rawType      = description.getRawType();
  SQLSMALLINT odbcTypeCode = TRINO_RAW_TYPE_TO_ODBC_TYPE_CODE[rawType];
  WriteLog(LL_TRACE,
           "  ODBC Type Code inferred as: " + std::to_string(odbcTypeCode));
  return odbcTypeCode;
}

/*
The work of SQLDescribeCol and SQLDescribeColW. The column name is
written for the kind of function that was called, and BufferLength is
a count of characters.
*/
static SQLRETURN describeColumn(SQLHSTMT StatementHandle,
                                SQLUSMALLINT ColumnNumber,
                                SQLPOINTER ColumnName,
                                SQLSMALLINT BufferLength,
                                SQLSMALLINT* NameLength,
                                SQLSMALLINT* DataType,
                                SQLULEN* ColumnSize,
                                SQLSMALLINT* DecimalDigits,
                                SQLSMALLINT* Nullable,
                                TextEncoding encoding) {
  WriteLog(LL_TRACE, "  Column index: " + std::to_string(ColumnNumber));

  Statement* statement = reinterpret_cast<Statement*>(StatementHandle);
  // A new call starts with no diagnostics from the previous one.
  statement->clearError();
  std::vector<ColumnDescription> columnDescriptions =
      statement->trinoQuery->getColumnDescriptions();

  ColumnDescription thisColumnDescription =
      columnDescriptions.at(ColumnNumber - 1);

  Descriptor* descriptorPtr       = statement->getRowDescriptor();
  DescriptorField descriptorField = descriptorPtr->getField(ColumnNumber);

  bool truncated = writeNullTermCharsToPtr(ColumnName,
                                           thisColumnDescription.getName(),
                                           BufferLength,
                                           NameLength,
                                           encoding);
  if (DataType) {
    *DataType = inferODBCTypeCode(thisColumnDescription);
  }
  if (ColumnSize) {
    // Unit is bytes for binary precision data.
    *ColumnSize = inferODBCColumnSize(thisColumnDescription);
  }
  if (Nullable) {
    // I don't know that trino provides a way to determine this based on
    // the underlying column metadata. It may be possible with additional
    // queries of metadata, but I don't think it's in the metadata
    // that comes with a normal SELECT statement. Assuming everything
    // is nullable seems like a reasonable default.
    *Nullable = SQL_NULLABLE;
  }
  if (DecimalDigits) {
    // The documentation says the value "0" should be set if the number of
    // digits cannot be determined or is not applicable. That's the default
    // value of the descriptorField struct's scale.
    *DecimalDigits = descriptorField.scale;
  }

  if (truncated) {
    WriteLog(LL_WARN,
             "  The buffer provided for the column name was too small");
    // 01004 = String data, right truncated. NameLength holds the
    // length the application needs to allocate to get the whole name.
    statement->setError(ErrorInfo("String data, right truncated", "01004"));
    return SQL_SUCCESS_WITH_INFO;
  }

  return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDescribeCol(SQLHSTMT StatementHandle,
                                 SQLUSMALLINT ColumnNumber,
                                 _Out_writes_opt_(BufferLength)
                                     SQLCHAR* ColumnName,
                                 SQLSMALLINT BufferLength,
                                 _Out_opt_ SQLSMALLINT* NameLength,
                                 _Out_opt_ SQLSMALLINT* DataType,
                                 _Out_opt_ SQLULEN* ColumnSize,
                                 _Out_opt_ SQLSMALLINT* DecimalDigits,
                                 _Out_opt_ SQLSMALLINT* Nullable) {
  WriteLog(LL_TRACE, "Entering SQLDescribeCol");
  return describeColumn(StatementHandle,
                        ColumnNumber,
                        ColumnName,
                        BufferLength,
                        NameLength,
                        DataType,
                        ColumnSize,
                        DecimalDigits,
                        Nullable,
                        AnsiText);
}

SQLRETURN SQL_API SQLDescribeColW(SQLHSTMT StatementHandle,
                                  SQLUSMALLINT ColumnNumber,
                                  _Out_writes_opt_(BufferLength)
                                      SQLWCHAR* ColumnName,
                                  SQLSMALLINT BufferLength,
                                  _Out_opt_ SQLSMALLINT* NameLength,
                                  _Out_opt_ SQLSMALLINT* DataType,
                                  _Out_opt_ SQLULEN* ColumnSize,
                                  _Out_opt_ SQLSMALLINT* DecimalDigits,
                                  _Out_opt_ SQLSMALLINT* Nullable) {
  WriteLog(LL_TRACE, "Entering SQLDescribeColW");
  return describeColumn(StatementHandle,
                        ColumnNumber,
                        ColumnName,
                        BufferLength,
                        NameLength,
                        DataType,
                        ColumnSize,
                        DecimalDigits,
                        Nullable,
                        UnicodeText);
}
