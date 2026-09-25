#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

#include "../trinoAPIWrapper/columnDescription.hpp"
#include "../util/rowToBuffer.hpp"
#include "../util/unicodeConversion.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"

using json = nlohmann::json;

/*
Return a value as text, either SQL_C_CHAR (UTF-8, as Trino sends it)
or SQL_C_WCHAR (UTF-16). Lengths are in bytes for both.

A value too long for the buffer is returned in parts. Each call writes
as much of what's left as fits, and returns SQL_SUCCESS_WITH_INFO with
SQLSTATE 01004 while more remains. The length reported is what was
left before the call. Once all of it has been returned, the next call
returns SQL_NO_DATA. Applications such as .NET read long strings this
way, and would get the start of the value over and over without it.
https://learn.microsoft.com/en-us/sql/odbc/reference/develop-app/getting-long-data
*/
static SQLRETURN getTextData(Statement* statement,
                             SQLUSMALLINT columnNumber,
                             SQLSMALLINT cDataType,
                             const json& value,
                             SQLPOINTER buffer,
                             SQLLEN bufferLength,
                             SQLLEN* strLen_or_IndPtr) {
  std::string text = jsonValueToText(value);
  if (getLogLevel() <= LL_TRACE) {
    WriteLog(LL_TRACE, "  Text value: " + text);
  }

  // Count in code units: bytes of UTF-8, or 16-bit units of UTF-16.
  std::u16string wideText;
  size_t unitSize   = 1;
  size_t totalUnits = text.size();
  if (cDataType == SQL_C_WCHAR) {
    wideText   = utf8ToUtf16(text);
    unitSize   = sizeof(char16_t);
    totalUnits = wideText.size();
  }

  // Carry on from the last call only if it read this same column, the
  // same way. Reading any other column starts over.
  if (statement->getDataColumn != columnNumber or
      statement->getDataCType != cDataType) {
    statement->resetGetDataPosition();
    statement->getDataColumn = columnNumber;
    statement->getDataCType  = cDataType;
  }
  if (statement->getDataFinished) {
    WriteLog(LL_TRACE, "  All of this value was already returned");
    return SQL_NO_DATA;
  }

  size_t offset    = statement->getDataOffset;
  size_t remaining = totalUnits - offset;
  if (strLen_or_IndPtr) {
    *strLen_or_IndPtr = static_cast<SQLLEN>(remaining * unitSize);
  }

  // One unit of the buffer belongs to the null terminator. Without
  // room for it, the application is only asking for the length, and
  // nothing is read.
  SQLLEN capacity = bufferLength / static_cast<SQLLEN>(unitSize);
  if (buffer == nullptr or capacity <= 0) {
    if (remaining > 0) {
      statement->setError(ErrorInfo("String data, right truncated", "01004"));
      return SQL_SUCCESS_WITH_INFO;
    }
    return SQL_SUCCESS;
  }

  size_t copied = std::min(remaining, static_cast<size_t>(capacity - 1));
  if (cDataType == SQL_C_WCHAR) {
    // Don't split a surrogate pair between two parts, unless the buffer
    // only has room for one code unit. Copying nothing then would return
    // the same empty part forever. The parts still join into valid UTF-16.
    if (copied < remaining and copied > 1 and
        wideText[offset + copied - 1] >= 0xD800 and
        wideText[offset + copied - 1] <= 0xDBFF) {
      copied--;
    }
    char16_t* wideBuffer = static_cast<char16_t*>(buffer);
    std::memcpy(wideBuffer, wideText.data() + offset, copied * unitSize);
    wideBuffer[copied] = u'\0';
  } else {
    char* charBuffer = static_cast<char*>(buffer);
    std::memcpy(charBuffer, text.data() + offset, copied);
    charBuffer[copied] = '\0';
  }
  statement->getDataOffset = offset + copied;

  if (copied < remaining) {
    WriteLog(LL_TRACE, "  Returning part of the value. More remains");
    statement->setError(ErrorInfo("String data, right truncated", "01004"));
    return SQL_SUCCESS_WITH_INFO;
  }
  statement->getDataFinished = true;
  return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetData(SQLHSTMT StatementHandle,
                             SQLUSMALLINT columnNumber,
                             SQLSMALLINT cDataType,
                             _Out_writes_opt_(_Inexpressible_(bufferLength))
                                 SQLPOINTER buffer,
                             SQLLEN bufferLength,
                             _Out_opt_ SQLLEN* strLen_or_IndPtr) {
  /*
  The cDataType is an indicator of the type of buffer that the application
  has resereved to receive the data from the driver. That may be different
  than the type of the column in the database.

  The columnNumber can be zero (0), which corresponds to the bookmark column.
  Column number one (1) is the first column of data returned by the
  query itself.

  bufferLength and strLen_or_IndPtr seems similar at first glance, but they're
  different in an important way. bufferLength is the size of the buffer
  the driver can use to copy data into. It's an input to this function.
  strLen_or_IndPtr is, in the case of a varchar, an output column that indicates
  the total length of data available in the source, which may be more than
  the size of the buffer provided by the application.
  */
  WriteLog(LL_TRACE, "Entering SQLGetData");
  Statement* statement = reinterpret_cast<Statement*>(StatementHandle);
  // A new call starts with no diagnostics from the previous one.
  statement->clearError();

  const std::vector<ColumnDescription>& columnDescriptions =
      statement->trinoQuery->getColumnDescriptions();

  // Make sure the column number is within the bounds of the columns
  // returned by the query.
  if (columnNumber < 1 || columnNumber > columnDescriptions.size()) {
    WriteLog(LL_ERROR, "  ERROR: SQLGetData - Column out of bounds");
    if (strLen_or_IndPtr) {
      *strLen_or_IndPtr = SQL_NULL_DATA;
    }
    if (buffer) {
      buffer = nullptr;
    }
    return SQL_ERROR;
  }

  Descriptor* rowDescriptor = statement->getRowDescriptor();
  const ColumnDescription& thisColumnDescription =
      columnDescriptions.at(columnNumber - 1);
  // The bookmark column is column 0
  DescriptorField descriptorField = rowDescriptor->getField(columnNumber);
  SQLSMALLINT odbcDataType        = descriptorField.odbcDataType;

  SQLLEN fetchedPosition = statement->getFetchedPosition();
  const json& rowData = statement->trinoQuery->getRowAtIndex(fetchedPosition);

  // Handle null data
  if (rowData[columnNumber - 1].is_null()) {
    if (strLen_or_IndPtr) {
      *strLen_or_IndPtr = SQL_NULL_DATA;
    }
    if (buffer) {
      buffer = nullptr;
    }
    WriteLog(LL_TRACE, "  Returning NULL for requested data");
    return SQL_SUCCESS;
  }

  if (getLogLevel() <= LL_TRACE) {
    WriteLog(LL_TRACE,
             "  Getting data for column: " + thisColumnDescription.getName());
    WriteLog(LL_TRACE, "  CDataType is: " + std::to_string(cDataType));
  }

  // Text is returned in parts when it doesn't fit, which the other
  // C types don't need.
  if (cDataType == SQL_C_CHAR or cDataType == SQL_C_WCHAR) {
    return getTextData(statement,
                       columnNumber,
                       cDataType,
                       rowData[columnNumber - 1],
                       buffer,
                       bufferLength,
                       strLen_or_IndPtr);
  }

  ColumnToBufferStatus status = columnToBuffer(cDataType,
                                               odbcDataType,
                                               rowData,
                                               columnNumber,
                                               buffer,
                                               bufferLength,
                                               strLen_or_IndPtr,
                                               descriptorField.precision,
                                               descriptorField.scale);

  // Nothing was written to the buffer or the length indicator, so
  // the application must not treat either as holding a value.
  if (not status.isSuccess) {
    ErrorInfo errorInfo =
        ErrorInfo("Restricted data type attribute violation: column " +
                      std::to_string(columnNumber) +
                      " cannot be read as C type " + std::to_string(cDataType),
                  "07006");
    statement->setError(errorInfo);
    return SQL_ERROR;
  }
  return SQL_SUCCESS;
}
