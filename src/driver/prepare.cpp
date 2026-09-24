#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/randomStr.hpp"
#include "../util/stringFromChar.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"

SQLRETURN SQL_API SQLPrepare(SQLHSTMT StatementHandle,
                             _In_reads_(TextLength) SQLCHAR* StatementText,
                             SQLINTEGER TextLength) {
  WriteLog(LL_DEBUG, "Entering SQLPrepare");

  if (not StatementText) {
    WriteLog(LL_ERROR, " ERROR: No StatementText defined for query");
    return SQL_ERROR;
  }

  Statement* statementPtr = reinterpret_cast<Statement*>(StatementHandle);
  // A new call starts with no diagnostics from the previous one.
  statementPtr->clearError();


  try {
    Statement* statement = reinterpret_cast<Statement*>(StatementHandle);
    // Bound parameters are kept. ODBC lets an application bind them
    // before or after SQLPrepare, and SQLExecute only passes as many
    // as the query has markers, so older bindings don't interfere.

    // Write the PREPARE statement for this query.
    std::string queryText = stringFromChar(StatementText, TextLength);
    WriteLog(LL_DEBUG, "  Raw Query: " + queryText);
    statement->statementText = queryText;
    statement->prepared      = false;
    std::string preparedName = getRandomText(12);
    std::string preparedQueryPrefix =
        std::format("PREPARE \"{}\" FROM", preparedName);
    std::string preparedQueryText = preparedQueryPrefix + "\n" + queryText;
    WriteLog(LL_DEBUG, "  Prepared Query: \n" + preparedQueryText);

    // Submit the prepared query to Trino for processing.
    TrinoQuery* trinoQuery = statement->trinoQuery;
    WriteLog(LL_DEBUG, "  Setting Prepared Query");
    trinoQuery->setQuery(preparedQueryText);
    WriteLog(LL_DEBUG, "  POSTing Prepared Query");
    trinoQuery->post();

    // Normally we would set the query to executed here after the POST,
    // but since this was only preparing the query we will not mark it
    // as executed yet. Instead we need to poll until trino has succesfully
    // prepared the query.
    trinoQuery->poll(UntilQueryPrepared);
    statement->prepared = true;

    return SQL_SUCCESS;

  } catch (const std::exception& ex) {
    WriteLog(LL_ERROR,
             "  ERROR: Exception thrown during SQLPrepare: " +
                 std::string(ex.what()));
    // Set the diagnostic record on the statement handle
    // to make it possible to tell what happened here.
    ErrorInfo errorInfo("Exception thrown during SQLPrepare: " +
                            std::string(ex.what()),
                        "HY000");
    statementPtr->setError(errorInfo);
    return SQL_ERROR;
  }
  // We should never get here, but let's have this in place just in case.
  return SQL_ERROR;
}
