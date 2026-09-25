#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/writeLog.hpp"

SQLRETURN SQL_API SQLSetCursorName(SQLHSTMT StatementHandle,
                                   _In_reads_(NameLength) SQLCHAR* CursorName,
                                   SQLSMALLINT NameLength) {
  WriteLog(LL_TRACE, "Entering SQLSetCursorName");
  WriteLog(LL_ERROR, "  ERROR: SQLSetCursorName is unimplemented");
  return SQL_ERROR;
}

SQLRETURN SQL_API SQLSetCursorNameW(SQLHSTMT StatementHandle,
                                    _In_reads_(NameLength) SQLWCHAR* CursorName,
                                    SQLSMALLINT NameLength) {
  WriteLog(LL_TRACE, "Entering SQLSetCursorNameW");
  WriteLog(LL_ERROR, "  ERROR: SQLSetCursorNameW is unimplemented");
  return SQL_ERROR;
}
