#include "../util/windowsLean.hpp"
#include <cstdint>
#include <sql.h>
#include <sqlext.h>

#include "../trinoAPIWrapper/trinoQuery.hpp"
#include "../util/writeLog.hpp"
#include "constants/statementAttrs.hpp"
#include "handles/statementHandle.hpp"

/*
Integer attributes are passed in Value itself, not through a pointer.
*/
static SQLULEN integerValue(SQLPOINTER Value) {
  return static_cast<SQLULEN>(reinterpret_cast<std::uintptr_t>(Value));
}

/*
For an attribute that this driver supports only one value of. Asking
for that value succeeds. Asking for any other value leaves the
attribute as it is, and ODBC calls that "option value changed". The
application can read the attribute back to see what's in effect.
*/
static SQLRETURN keepSupportedValue(Statement* statement,
                                    SQLINTEGER Attribute,
                                    SQLULEN requestedValue,
                                    SQLULEN supportedValue) {
  if (requestedValue == supportedValue) {
    return SQL_SUCCESS;
  }
  WriteLog(LL_WARN,
           "  Attribute " + std::to_string(Attribute) + " can't be set to " +
               std::to_string(requestedValue) + ", keeping " +
               std::to_string(supportedValue));
  // 01S02 = Option value changed.
  statement->setError(ErrorInfo("Option value changed", "01S02"));
  return SQL_SUCCESS_WITH_INFO;
}

SQLRETURN SQL_API SQLSetStmtAttr(SQLHSTMT StatementHandle,
                                 SQLINTEGER Attribute,
                                 _In_reads_(_Inexpressible_(StringLength))
                                     SQLPOINTER Value,
                                 SQLINTEGER StringLength) {
  WriteLog(LL_TRACE, "Entering SQLSetStmtAttr");
  Statement* statement = reinterpret_cast<Statement*>(StatementHandle);
  // A new call starts with no diagnostics from the previous one.
  statement->clearError();

  WriteLog(LL_TRACE, "  Setting attribute: " + std::to_string(Attribute));
  switch (Attribute) {
    case SQL_ATTR_QUERY_TIMEOUT: // 0
    case SQL_ATTR_MAX_ROWS:      // 1
    case SQL_ATTR_MAX_LENGTH:    // 3
    case SQL_ATTR_KEYSET_SIZE: { // 8
      // No timeout and no limits. Zero means none for all of these.
      return keepSupportedValue(statement, Attribute, integerValue(Value), 0);
    }
    case SQL_ATTR_NOSCAN: { // 2
      // Escape sequences are never scanned for.
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_NOSCAN_OFF);
    }
    case SQL_ATTR_ASYNC_ENABLE: { // 4
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_ASYNC_ENABLE_OFF);
    }
    case SQL_ATTR_ROW_BIND_TYPE: { // 5
      // With one row per fetch, row-wise binding puts every column
      // where column-wise binding would, so either one works.
      statement->getRowDescriptor()->Field_BindType =
          static_cast<SQLUINTEGER>(integerValue(Value));
      break;
    }
    case SQL_ATTR_CURSOR_TYPE: { // 6
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_CURSOR_FORWARD_ONLY);
    }
    case SQL_ATTR_CONCURRENCY: { // 7
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_CONCUR_READ_ONLY);
    }
    case SQL_ROWSET_SIZE:           // 9
    case SQL_ATTR_ROW_ARRAY_SIZE: { // 27
      // One row per fetch.
      return keepSupportedValue(statement, Attribute, integerValue(Value), 1);
    }
    case SQL_ATTR_RETRIEVE_DATA: { // 11
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_RD_ON);
    }
    case SQL_ATTR_USE_BOOKMARKS: { // 12
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_UB_OFF);
    }
    case SQL_ATTR_ENABLE_AUTO_IPD: { // 15
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_FALSE);
    }
    case SQL_ATTR_PARAM_BIND_TYPE: { // 18
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_PARAM_BIND_BY_COLUMN);
    }
    case SQL_ATTR_PARAMSET_SIZE: { // 22
      // One set of parameters per execution.
      return keepSupportedValue(statement, Attribute, integerValue(Value), 1);
    }
    case SQL_ATTR_ROW_BIND_OFFSET_PTR: { // 23
      statement->getRowDescriptor()->Field_BindOffsetPtr =
          static_cast<SQLLEN*>(Value);
      break;
    }
    case SQL_ATTR_ROW_STATUS_PTR: { // 25
      statement->impRowDesc->Field_ArrayStatusPtr =
          static_cast<SQLUSMALLINT*>(Value);
      break;
    }
    case SQL_ATTR_CURSOR_SCROLLABLE: { // -1
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_NONSCROLLABLE);
    }
    case SQL_ATTR_CURSOR_SENSITIVITY: { // -2
      // Leaving it unspecified is fine too.
      if (integerValue(Value) == SQL_UNSPECIFIED) {
        break;
      }
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_INSENSITIVE);
    }
    case SQL_ATTR_METADATA_ID: { // 10014
      // Catalog function arguments are always patterns, not identifiers.
      return keepSupportedValue(
          statement, Attribute, integerValue(Value), SQL_FALSE);
    }
    case SQL_ATTR_ROWS_FETCHED_PTR: { // 26
      SQLULEN* rowsProcessedPtr = static_cast<SQLULEN*>(Value);
      WriteLog(LL_TRACE, std::format("  Attribute value is set to {}", Value));
      Descriptor* impRowDesc             = statement->impRowDesc;
      impRowDesc->Field_RowsProcessedPtr = rowsProcessedPtr;
      break;
    }
    case SQL_ATTR_DEFAULT_FETCH_POLL_MODE: { // 1002
      SQLINTEGER pollModeInt = *reinterpret_cast<SQLINTEGER*>(Value);
      WriteLog(LL_TRACE,
               "  Attribute value is set to " + std::to_string(pollModeInt));
      statement->fetchPollMode = static_cast<TrinoQueryPollMode>(pollModeInt);
      break;
    }
    default: {
      WriteLog(LL_ERROR,
               "  ERROR: Attribute " + std::to_string(Attribute) +
                   " is not implemented");
      // HY092 = Invalid attribute/option identifier.
      statement->setError(ErrorInfo("Unknown Statement Attribute", "HY092"));
      return SQL_ERROR;
    }
  }

  return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetStmtAttrW(SQLHSTMT StatementHandle,
                                  SQLINTEGER Attribute,
                                  _In_reads_(_Inexpressible_(StringLength))
                                      SQLPOINTER Value,
                                  SQLINTEGER StringLength) {
  // None of the attributes this driver supports are strings, so the
  // Unicode version has nothing to convert.
  WriteLog(LL_TRACE, "Entering SQLSetStmtAttrW");
  return SQLSetStmtAttr(StatementHandle, Attribute, Value, StringLength);
}
