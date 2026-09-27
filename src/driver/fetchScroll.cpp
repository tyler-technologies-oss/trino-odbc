#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"

SQLRETURN SQL_API SQLFetchScroll(SQLHSTMT StatementHandle,
                                 SQLSMALLINT FetchOrientation,
                                 SQLLEN FetchOffset) {
  WriteLog(LL_TRACE, "Entering SQLFetchScroll");
  if (!StatementHandle) {
    WriteLog(LL_ERROR, "  ERROR: Invalid statement handle");
    return SQL_INVALID_HANDLE;
  }

  // Cursors are forward-only, so the next row is the only one there
  // is to fetch. SQLFetch does exactly that.
  if (FetchOrientation == SQL_FETCH_NEXT) {
    return SQLFetch(StatementHandle);
  }

  WriteLog(LL_ERROR,
           "  ERROR: Unsupported fetch orientation: " +
               std::to_string(FetchOrientation));
  Statement* statement = reinterpret_cast<Statement*>(StatementHandle);
  // HY106 = Fetch type out of range.
  statement->setError(ErrorInfo("Only SQL_FETCH_NEXT is supported", "HY106"));
  return SQL_ERROR;
}
