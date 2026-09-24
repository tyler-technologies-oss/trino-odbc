#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/parameterMarkers.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"

SQLRETURN SQL_API SQLNumParams(SQLHSTMT hstmt, _Out_opt_ SQLSMALLINT* pcpar) {
  WriteLog(LL_TRACE, "Entering SQLNumParams");
  Statement* statement = reinterpret_cast<Statement*>(hstmt);
  // The parameters are the markers in the query given to SQLPrepare.
  if (pcpar != nullptr) {
    *pcpar = static_cast<SQLSMALLINT>(
        countParameterMarkers(statement->preparedQuery));
  }
  return SQL_SUCCESS;
}
