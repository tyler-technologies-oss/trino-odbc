#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/parameterMarkers.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"
#include "mappings/parameterToText.hpp"

SQLRETURN SQL_API SQLExecute(SQLHSTMT StatementHandle) {
  WriteLog(LL_DEBUG, "Entering SQLExecute");

  Statement* statementPtr = reinterpret_cast<Statement*>(StatementHandle);
  // A new call starts with no diagnostics from the previous one. That
  // includes the error from the last Trino query, which would otherwise
  // be reported again if this call fails before posting a new one.
  // The prepared statement itself survives, since Trino tracks it in
  // connection headers that this doesn't touch.
  statementPtr->clearError();
  statementPtr->trinoQuery->reset();
  statementPtr->executed = false;

  try {
    Statement* statement = reinterpret_cast<Statement*>(StatementHandle);
    if (not statement->prepared) {
      WriteLog(LL_ERROR, "  ERROR: SQLExecute called with no prepared query");
      ErrorInfo errorInfo("SQLExecute was called without a prepared statement",
                          "HY010");
      statementPtr->setError(errorInfo);
      return SQL_ERROR;
    }
    std::string lastPreparedStatementName =
        statement->trinoQuery->getLastPreparedStatementName();
    std::string query =
        std::format("EXECUTE \"{}\"\n", lastPreparedStatementName);

    // Pass one bound parameter for each marker in the prepared query.
    // Bindings outlive the statement they were made for, so there may
    // be more of them than markers, and Trino rejects any extras.
    SQLSMALLINT markerCount = static_cast<SQLSMALLINT>(
        countParametersToBind(statement->statementText));
    Descriptor* paramDescriptor = statement->getParamDescriptor();
    if (markerCount > 0) {
      if (countBoundParameters(paramDescriptor) < markerCount) {
        WriteLog(LL_ERROR, "  ERROR: Not every parameter marker is bound");
        ErrorInfo errorInfo(
            std::format("The query has {} parameter markers, but only the "
                        "first {} are bound",
                        markerCount,
                        countBoundParameters(paramDescriptor)),
            "07002");
        statementPtr->setError(errorInfo);
        return SQL_ERROR;
      }
      query += "USING " + parameterListText(paramDescriptor, markerCount);
    }
    WriteLog(LL_DEBUG, "  Executed Prepared Query: " + query);
    TrinoQuery* trinoQuery = statement->trinoQuery;
    WriteLog(LL_DEBUG, "  Setting Prepared Query");
    trinoQuery->setQuery(query);
    WriteLog(LL_DEBUG, "  POSTing Prepared Query");
    trinoQuery->post();
    // As in SQLExecDirect, wait until Trino has either returned the
    // columns or finished, so a failed EXECUTE is reported here.
    WriteLog(LL_DEBUG, "  Polling until columns are loaded");
    trinoQuery->poll(UntilColumnsLoaded);
    if (trinoQuery->hasError()) {
      WriteLog(LL_ERROR, "  ERROR: Trino rejected the prepared query");
      return SQL_ERROR;
    }
    WriteLog(LL_DEBUG, "  Setting prepared query to executed");
    statement->executed = true;
    return SQL_SUCCESS;
  } catch (const std::exception& ex) {
    WriteLog(LL_ERROR,
             "  ERROR: Exception thrown during SQLExecute: " +
                 std::string(ex.what()));
    // Set the diagnostic record on the statement handle
    // to make it possible to tell what happened here.
    ErrorInfo errorInfo("Exception thrown during SQLExecute: " +
                            std::string(ex.what()),
                        "HY000");
    statementPtr->setError(errorInfo);
    return SQL_ERROR;
  }
  // We should never get here, but let's have this in place just in case.
  return SQL_ERROR;
}
