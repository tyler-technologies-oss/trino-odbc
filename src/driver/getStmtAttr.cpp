#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/writeLog.hpp"
#include "constants/statementAttrs.hpp"
#include "handles/statementHandle.hpp"

SQLRETURN SQL_API SQLGetStmtAttr(SQLHSTMT StatementHandle,
                                 SQLINTEGER Attribute,
                                 _Out_writes_opt_(_Inexpressible_(BufferLength))
                                     SQLPOINTER Value,
                                 SQLINTEGER BufferLength,
                                 _Out_opt_ SQLINTEGER* StringLength) {
  WriteLog(LL_TRACE, "Entering SQLGetStmtAttr");
  if (!StatementHandle) {
    WriteLog(LL_ERROR, "  ERROR: Invalid statement handle");
    if (Value) {
      Value = nullptr;
    }
    if (StringLength) {
      StringLength = 0;
    }
    return SQL_INVALID_HANDLE;
  }

  Statement* statement = reinterpret_cast<Statement*>(StatementHandle);
  // A new call starts with no diagnostics from the previous one.
  statement->clearError();

  // Most attributes hold a single fixed value, since this driver
  // supports only one choice for them. See SQLSetStmtAttr.
  bool isFixedValue  = true;
  SQLULEN fixedValue = 0;
  switch (Attribute) {
    case SQL_ATTR_QUERY_TIMEOUT: // 0
    case SQL_ATTR_MAX_ROWS:      // 1
    case SQL_ATTR_MAX_LENGTH:    // 3
    case SQL_ATTR_KEYSET_SIZE: { // 8
      fixedValue = 0;
      break;
    }
    case SQL_ATTR_NOSCAN: { // 2
      fixedValue = SQL_NOSCAN_OFF;
      break;
    }
    case SQL_ATTR_ASYNC_ENABLE: { // 4
      fixedValue = SQL_ASYNC_ENABLE_OFF;
      break;
    }
    case SQL_ATTR_ROW_BIND_TYPE: { // 5
      fixedValue = statement->getRowDescriptor()->Field_BindType;
      break;
    }
    case SQL_ATTR_CURSOR_TYPE: { // 6
      fixedValue = SQL_CURSOR_FORWARD_ONLY;
      break;
    }
    case SQL_ATTR_CONCURRENCY: { // 7
      fixedValue = SQL_CONCUR_READ_ONLY;
      break;
    }
    case SQL_ROWSET_SIZE:          // 9
    case SQL_ATTR_ROW_ARRAY_SIZE:  // 27
    case SQL_ATTR_PARAMSET_SIZE: { // 22
      fixedValue = 1;
      break;
    }
    case SQL_ATTR_RETRIEVE_DATA: { // 11
      fixedValue = SQL_RD_ON;
      break;
    }
    case SQL_ATTR_USE_BOOKMARKS: { // 12
      fixedValue = SQL_UB_OFF;
      break;
    }
    case SQL_ATTR_ENABLE_AUTO_IPD: // 15
    case SQL_ATTR_METADATA_ID: {   // 10014
      fixedValue = SQL_FALSE;
      break;
    }
    case SQL_ATTR_PARAM_BIND_TYPE: { // 18
      fixedValue = SQL_PARAM_BIND_BY_COLUMN;
      break;
    }
    case SQL_ATTR_CURSOR_SCROLLABLE: { // -1
      fixedValue = SQL_NONSCROLLABLE;
      break;
    }
    case SQL_ATTR_CURSOR_SENSITIVITY: { // -2
      fixedValue = SQL_INSENSITIVE;
      break;
    }
    default: {
      isFixedValue = false;
      break;
    }
  }
  if (isFixedValue) {
    // These are integers, so BufferLength doesn't apply. ODBC defines
    // them all as SQLULEN, apart from a few SQLUINTEGERs, and writing
    // the smaller type would leave half of a SQLULEN untouched.
    if (Value) {
      *reinterpret_cast<SQLULEN*>(Value) = fixedValue;
    }
    if (StringLength) {
      *StringLength = sizeof(SQLULEN);
    }
    WriteLog(LL_TRACE,
             "  Finished getting attribute: " + std::to_string(Attribute));
    return SQL_SUCCESS;
  }

  switch (Attribute) {
    case SQL_ATTR_ROW_BIND_OFFSET_PTR: { // 23
      if (Value) {
        *reinterpret_cast<SQLLEN**>(Value) =
            statement->getRowDescriptor()->Field_BindOffsetPtr;
      }
      if (StringLength) {
        *StringLength = sizeof(SQLLEN*);
      }
      break;
    }
    case SQL_ATTR_ROW_STATUS_PTR: { // 25
      if (Value) {
        *reinterpret_cast<SQLUSMALLINT**>(Value) =
            statement->impRowDesc->Field_ArrayStatusPtr;
      }
      if (StringLength) {
        *StringLength = sizeof(SQLUSMALLINT*);
      }
      break;
    }
    case SQL_ATTR_ROWS_FETCHED_PTR: { // 26
      if (Value) {
        *reinterpret_cast<SQLULEN**>(Value) =
            statement->impRowDesc->Field_RowsProcessedPtr;
      }
      if (StringLength) {
        *StringLength = sizeof(SQLULEN*);
      }
      break;
    }
    case SQL_ATTR_ROW_NUMBER: { // 14
      if (Value) {
        *reinterpret_cast<SQLULEN*>(Value) = statement->getFetchedPosition();
      }
      if (StringLength) {
        *StringLength = sizeof(SQLULEN*);
      }
      break;
    }
    case SQL_ATTR_APP_ROW_DESC: { // 10010
      if (Value) {
        *reinterpret_cast<SQLPOINTER*>(Value) = statement->appRowDesc;
      }
      if (StringLength) {
        *StringLength = sizeof(SQLPOINTER*);
      }
      break;
    }
    case SQL_ATTR_APP_PARAM_DESC: { // 10011
      if (Value) {
        *reinterpret_cast<SQLPOINTER*>(Value) = statement->appParamDesc;
      }
      if (StringLength) {
        *StringLength = sizeof(SQLPOINTER*);
      }
      break;
    }
    case SQL_ATTR_IMP_ROW_DESC: { // 10012
      if (Value) {
        *reinterpret_cast<SQLPOINTER*>(Value) = statement->impRowDesc;
      }
      if (StringLength) {
        *StringLength = sizeof(SQLPOINTER*);
      }
      break;
    }
    case SQL_ATTR_IMP_PARAM_DESC: { // 10013
      if (Value) {
        *reinterpret_cast<SQLPOINTER*>(Value) = statement->impParamDesc;
      }
      if (StringLength) {
        *StringLength = sizeof(SQLPOINTER*);
      }
      break;
    }
    case SQL_ATTR_RAW_STATEMENT_HANDLE: { // 1001
      // Driver defined - get raw statement handle, no ODBC proxy.
      if (Value) {
        *reinterpret_cast<SQLPOINTER*>(Value) = StatementHandle;
      }
      if (StringLength) {
        *StringLength = sizeof(SQLPOINTER*);
      }
      break;
    }
    default: {
      WriteLog(LL_ERROR,
               "  ERROR: Unsupported attribute: " + std::to_string(Attribute));
      // BufferLength is only a byte count when it's positive. Integer
      // attributes pass 0 or a negative SQL_IS_* marker instead.
      if (Value && BufferLength > 0) {
        memset(Value, 0, BufferLength);
      }
      if (StringLength) {
        *StringLength = 0;
      }
      ErrorInfo errorInfo("Unknown Statement Attribute", "HY092");
      statement->setError(errorInfo);
      return SQL_ERROR;
    }
  }
  WriteLog(LL_TRACE,
           "  Finished getting attribute: " + std::to_string(Attribute));
  return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetStmtAttrW(
    SQLHSTMT StatementHandle,
    SQLINTEGER Attribute,
    _Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
    SQLINTEGER BufferLength,
    _Out_opt_ SQLINTEGER* StringLength) {
  // None of the attributes this driver supports are strings, so the
  // Unicode version has nothing to convert.
  WriteLog(LL_TRACE, "Entering SQLGetStmtAttrW");
  return SQLGetStmtAttr(
      StatementHandle, Attribute, Value, BufferLength, StringLength);
}
