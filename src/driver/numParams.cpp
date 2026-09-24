#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/parameterMarkers.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"

SQLRETURN SQL_API SQLNumParams(SQLHSTMT hstmt, _Out_opt_ SQLSMALLINT* pcpar) {
  WriteLog(LL_TRACE, "Entering SQLNumParams");
  Statement* statement = reinterpret_cast<Statement*>(hstmt);
  // A new call starts with no diagnostics from the previous one.
  statement->clearError();

  // There is nothing to count until SQLPrepare or SQLExecDirect has
  // given the handle a statement, and ODBC calls that a sequence error.
  if (statement->statementText.empty()) {
    WriteLog(LL_ERROR, "  ERROR: SQLNumParams called with no statement");
    ErrorInfo errorInfo(
        "SQLNumParams was called before SQLPrepare or SQLExecDirect", "HY010");
    statement->setError(errorInfo);
    return SQL_ERROR;
  }

  // The parameters are the markers that bound values fill, in the
  // statement given to the latest SQLPrepare or SQLExecDirect.
  if (pcpar != nullptr) {
    *pcpar = static_cast<SQLSMALLINT>(
        countParametersToBind(statement->statementText));
  }
  return SQL_SUCCESS;
}
