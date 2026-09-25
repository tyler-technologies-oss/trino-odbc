#include "../util/windowsLean.hpp"
#include <sql.h>
#include <string.h>

#include <format>

#include "../trinoAPIWrapper/trinoQuery.hpp"
#include "../util/parameterMarkers.hpp"
#include "../util/stringFromChar.hpp"
#include "../util/stringReplace.hpp"
#include "../util/stringTrim.hpp"
#include "../util/unicodeConversion.hpp"
#include "../util/writeLog.hpp"
#include "handles/statementHandle.hpp"
#include "mappings/parameterToText.hpp"

SQLRETURN SQL_API SQLExecDirect(SQLHSTMT StatementHandle,
                                _In_reads_opt_(TextLength)
                                    SQLCHAR* StatementText,
                                SQLINTEGER TextLength) {
  WriteLog(LL_TRACE, "Entering SQLExecDirect");

  if (not StatementText) {
    WriteLog(LL_ERROR, " ERROR: No StatementText defined for query");
    return SQL_ERROR;
  }

  Statement* statementPtr = reinterpret_cast<Statement*>(StatementHandle);
  // A new call starts with no diagnostics from the previous one. That
  // includes the error from the last Trino query, which would otherwise
  // be reported again if this call fails before posting a new one.
  statementPtr->clearError();
  statementPtr->trinoQuery->reset();
  statementPtr->executed = false;

  try {
    Statement* statement = (Statement*)StatementHandle;
    std::string queryText =
        removeTrailingSemicolons(stringFromChar(StatementText, TextLength));
    WriteLog(LL_DEBUG, "  Query: " + queryText);
    // This replaces any prepared statement on the handle.
    statement->statementText = queryText;
    statement->prepared      = false;

    // Applications such as Report Builder bind parameters and then
    // call SQLExecDirect, without SQLPrepare. Trino only accepts
    // parameter values through EXECUTE, so run the query with
    // EXECUTE IMMEDIATE and pass the bound values after USING.
    // A PREPARE statement is always sent as it is, since its markers
    // are filled by a later EXECUTE and not by anything bound now.
    // So is a query with markers but no bound parameters.
    SQLSMALLINT markerCount =
        static_cast<SQLSMALLINT>(countParametersToBind(queryText));
    Descriptor* paramDescriptor = statement->getParamDescriptor();
    SQLSMALLINT boundCount      = countBoundParameters(paramDescriptor);
    if (markerCount > 0 and boundCount > 0) {
      if (boundCount < markerCount) {
        WriteLog(LL_ERROR, "  ERROR: Not every parameter marker is bound");
        ErrorInfo errorInfo(
            std::format("The query has {} parameter markers, but only the "
                        "first {} are bound",
                        markerCount,
                        boundCount),
            "07002");
        statementPtr->setError(errorInfo);
        return SQL_ERROR;
      }
      queryText = std::format("EXECUTE IMMEDIATE '{}'\nUSING {}",
                              replaceAll(queryText, "'", "''"),
                              parameterListText(paramDescriptor, markerCount));
      WriteLog(LL_DEBUG, "  Query with parameters: " + queryText);
    }
    TrinoQuery* trinoQuery = statement->trinoQuery;
    WriteLog(LL_DEBUG, "  Setting Query");
    trinoQuery->setQuery(queryText);
    WriteLog(LL_DEBUG, "  POSTing Query");
    trinoQuery->post();
    // Trino accepts the POST before it has looked at the query, and
    // reports a bad query on a later poll. Wait for the columns (or
    // the end of the query) so a rejected query fails here, where
    // applications check for it, instead of looking like an empty result.
    WriteLog(LL_DEBUG, "  Polling until columns are loaded");
    trinoQuery->poll(UntilColumnsLoaded);
    if (trinoQuery->hasError()) {
      WriteLog(LL_ERROR, "  ERROR: Trino rejected the query");
      return SQL_ERROR;
    }
    WriteLog(LL_DEBUG, "  Setting to executed");
    statement->executed = true;
    return SQL_SUCCESS;
  } catch (const std::exception& ex) {
    WriteLog(LL_ERROR,
             "  ERROR: Exception thrown during SQLExecDirect: " +
                 std::string(ex.what()));
    // Set the diagnostic record on the statement handle
    // to make it possible to tell what happened here.
    ErrorInfo errorInfo("Exception thrown during SQLExecDirect: " +
                            std::string(ex.what()),
                        "HY000");
    statementPtr->setError(errorInfo);
    return SQL_ERROR;
  }
}

SQLRETURN SQL_API SQLExecDirectW(SQLHSTMT StatementHandle,
                                 _In_reads_opt_(TextLength)
                                     SQLWCHAR* StatementText,
                                 SQLINTEGER TextLength) {
  WriteLog(LL_TRACE, "Entering SQLExecDirectW");
  if (not StatementText) {
    return SQLExecDirect(StatementHandle, nullptr, TextLength);
  }
  std::string queryText = stringFromWideChar(
      reinterpret_cast<char16_t*>(StatementText), TextLength);
  return SQLExecDirect(StatementHandle,
                       reinterpret_cast<SQLCHAR*>(queryText.data()),
                       static_cast<SQLINTEGER>(queryText.size()));
}
