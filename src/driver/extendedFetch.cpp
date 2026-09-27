#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"

/*
The ODBC 2 version of SQLFetchScroll. It returns the row count and row
status through arguments rather than statement attributes.
*/
SQLRETURN SQL_API SQLExtendedFetch(SQLHSTMT hstmt,
                                   SQLUSMALLINT fFetchType,
                                   SQLLEN irow,
                                   _Out_opt_ SQLULEN* pcrow,
                                   _Out_opt_ SQLUSMALLINT* rgfRowStatus) {
  WriteLog(LL_TRACE, "Entering SQLExtendedFetch");
  if (!hstmt) {
    WriteLog(LL_ERROR, "  ERROR: Invalid statement handle");
    return SQL_INVALID_HANDLE;
  }

  if (fFetchType != SQL_FETCH_NEXT) {
    WriteLog(LL_ERROR,
             "  ERROR: Unsupported fetch type: " + std::to_string(fFetchType));
    Statement* statement = reinterpret_cast<Statement*>(hstmt);
    // HY106 = Fetch type out of range.
    statement->setError(ErrorInfo("Only SQL_FETCH_NEXT is supported", "HY106"));
    return SQL_ERROR;
  }

  // Cursors are forward-only, and one row is fetched at a time.
  SQLRETURN ret = SQLFetch(hstmt);
  if (ret == SQL_SUCCESS or ret == SQL_SUCCESS_WITH_INFO) {
    if (pcrow) {
      *pcrow = 1;
    }
    if (rgfRowStatus) {
      rgfRowStatus[0] = SQL_ROW_SUCCESS;
    }
  } else if (ret == SQL_NO_DATA) {
    if (pcrow) {
      *pcrow = 0;
    }
  }
  return ret;
}
