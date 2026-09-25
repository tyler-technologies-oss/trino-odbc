#include <windows.h>

#include <gtest/gtest.h>
#include <iostream>
#include <sql.h>
#include <sqlext.h>

#include "../constants.hpp"

#include "../fixtures/sqlDriverConnectFixture.hpp"

class SQLPrepareTest : public SQLDriverConnectFixture {};

TEST_F(SQLPrepareTest, TestPreparedExecutionWithoutParameters) {
  // Allocate statement handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Prepare a SQL statement
  std::string query = R"SQL(
      SELECT *
      FROM tpch.sf1.customer
      WHERE custkey = 42
  )SQL";
  ret               = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);


  // Execute the statement.
  ret = SQLExecute(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Fetch results for custkey 1
  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Get a cell of data from the result.
  SQLBIGINT resultOne;
  ret =
      SQLGetData(hStmt, 1, SQL_C_SBIGINT, &resultOne, sizeof(resultOne), NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // The result is the customer key, we're just selecting out
  // the same key we used for querying. It's a simple test, but
  // it verifies the thing we want to verify here.
  ASSERT_EQ(resultOne, 42);

  // Free statement handle
  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}


// Power BI reads the result columns between SQLPrepare and SQLExecute,
// so they must be available before the query runs.
TEST_F(SQLPrepareTest, TestColumnsAreDescribedBeforeExecute) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = R"SQL(
      SELECT custkey, name, acctbal
      FROM tpch.sf1.customer
      WHERE custkey = ?
  )SQL";
  ret               = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLSMALLINT columnCount = 0;
  ret                     = SQLNumResultCols(hStmt, &columnCount);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(columnCount, 3);

  SQLCHAR columnName[64];
  SQLSMALLINT nameLength;
  SQLSMALLINT dataType;
  SQLULEN columnSize;
  SQLSMALLINT decimalDigits;
  SQLSMALLINT nullable;
  ret = SQLDescribeCol(hStmt,
                       2,
                       columnName,
                       sizeof(columnName),
                       &nameLength,
                       &dataType,
                       &columnSize,
                       &decimalDigits,
                       &nullable);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(columnName), "name");
  ASSERT_EQ(dataType, SQL_VARCHAR);

  ret = SQLDescribeCol(hStmt,
                       3,
                       columnName,
                       sizeof(columnName),
                       &nameLength,
                       &dataType,
                       &columnSize,
                       &decimalDigits,
                       &nullable);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(columnName), "acctbal");
  ASSERT_EQ(dataType, SQL_DOUBLE);

  // The query still runs as usual afterwards.
  SQLINTEGER custkey = 42;
  ret                = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_SLONG,
                         SQL_INTEGER,
                         0,
                         0,
                         &custkey,
                         0,
                         NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLExecute(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  SQLBIGINT result = 0;
  ret = SQLGetData(hStmt, 1, SQL_C_SBIGINT, &result, sizeof(result), NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(result, 42);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// Column bindings last until SQLFreeStmt(SQL_UNBIND), so binding
// before SQLPrepare must still fill the buffer on SQLFetch.
TEST_F(SQLPrepareTest, TestColumnBindingsSurvivePrepare) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLBIGINT custkey = 0;
  SQLLEN indicator  = 0;
  ret               = SQLBindCol(
      hStmt, 1, SQL_C_SBIGINT, &custkey, sizeof(custkey), &indicator);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = R"SQL(
      SELECT custkey, name
      FROM tpch.sf1.customer
      WHERE custkey = 42
  )SQL";
  ret               = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLExecute(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(custkey, 42);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// SQLFetch on a statement with no query used to recurse until the
// stack overflowed. This needs no server.
TEST_F(SQLPrepareTest, TestFetchWithoutExecuteFails) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_ERROR);

  SQLCHAR sqlState[SQL_SQLSTATE_SIZE + 1] = {0};
  SQLCHAR message[256]                    = {0};
  SQLINTEGER nativeError                  = 0;
  SQLSMALLINT messageLen                  = 0;
  ret                                     = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLen);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(sqlState), "HY010");

  // A failed SQLExecute leaves no query to fetch from either.
  ret = SQLExecute(hStmt);
  ASSERT_EQ(ret, SQL_ERROR);
  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_ERROR);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLPrepareTest, TestPreparedExecutionWithInputParameters) {
  // Allocate statement handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Prepare a SQL statement. For the purposes of this test, we're adding
  // a second parameter that's a string instead of an integer.
  // This exercises the code paths for multiple parameters and multiple
  // types of parameters instead of just one.
  std::string query = R"SQL(
      SELECT *
      FROM tpch.sf1.customer
      WHERE true
         AND custkey = ?
         AND name LIKE ?
  )SQL";
  ret               = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Bind the first parameter
  SQLINTEGER custkeyParam;

  ret = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_SLONG,
                         SQL_INTEGER,
                         0,
                         0,
                         &custkeyParam,
                         0,
                         NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Bind the second parameter
  SQLCHAR nameLikeParam[16] = {'\0'};

  ret = SQLBindParameter(hStmt,
                         2,
                         SQL_PARAM_INPUT,
                         SQL_C_CHAR,
                         SQL_CHAR,
                         0,
                         0,
                         nameLikeParam,
                         sizeof(nameLikeParam),
                         NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);


  // Execute the statement for custkey 1. The
  // two keys should be the same.
  custkeyParam           = 42;
  std::string likeString = "Customer#%";
  strncpy_s(reinterpret_cast<char*>(nameLikeParam),
            sizeof(nameLikeParam),
            likeString.c_str(),
            likeString.size());
  ret = SQLExecute(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Fetch results for custkey 1
  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Get some data from the result.
  SQLBIGINT resultOne = 0;
  ret =
      SQLGetData(hStmt, 1, SQL_C_SBIGINT, &resultOne, sizeof(resultOne), NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // The result is the customer key, we're just selecting out
  // the same key we used for querying. It's a simple test, but
  // it verifies the thing we want to verify here.
  ASSERT_EQ(custkeyParam, resultOne);

  // Free statement handle
  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// Report Builder, and other .NET applications, bind parameters and
// then call SQLExecDirect without ever calling SQLPrepare. They bind
// dates as SQL_C_TYPE_TIMESTAMP and strings as SQL_C_WCHAR.
TEST_F(SQLPrepareTest, TestExecDirectWithBoundTimestampParameters) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQL_TIMESTAMP_STRUCT startParam = {2026, 9, 1, 0, 0, 0, 0};
  SQL_TIMESTAMP_STRUCT endParam   = {2026, 9, 24, 0, 0, 0, 0};
  ret                             = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_TYPE_TIMESTAMP,
                         SQL_TYPE_TIMESTAMP,
                         23,
                         3,
                         &startParam,
                         sizeof(startParam),
                         NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLBindParameter(hStmt,
                         2,
                         SQL_PARAM_INPUT,
                         SQL_C_TYPE_TIMESTAMP,
                         SQL_TYPE_TIMESTAMP,
                         23,
                         3,
                         &endParam,
                         sizeof(endParam),
                         NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = "SELECT date_diff('day', cast(? as timestamp), "
                      "cast(? as timestamp))";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLBIGINT days = 0;
  ret = SQLGetData(hStmt, 1, SQL_C_SBIGINT, &days, sizeof(days), NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(days, 23);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLPrepareTest, TestExecDirectWithBoundWideStringParameter) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // The quote checks that the value is escaped inside EXECUTE IMMEDIATE.
  SQLWCHAR textParam[] = {'O', '\'', 'B', 'r', 'i', 'e', 'n', 0};
  SQLLEN textLength    = 7 * sizeof(SQLWCHAR);
  ret                  = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_WCHAR,
                         SQL_WVARCHAR,
                         7,
                         0,
                         textParam,
                         sizeof(textParam),
                         &textLength);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // The query's own string literal checks it survives EXECUTE IMMEDIATE.
  std::string query = "SELECT concat(?, ' isn''t ?')";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  char result[64] = {'\0'};
  ret = SQLGetData(hStmt, 1, SQL_C_CHAR, result, sizeof(result), NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(result, "O'Brien isn't ?");

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLPrepareTest, TestExecDirectWithTooFewBoundParameters) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLINTEGER value = 1;
  ret              = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_SLONG,
                         SQL_INTEGER,
                         0,
                         0,
                         &value,
                         0,
                         NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // The driver notices the missing parameter without asking Trino.
  std::string query = "SELECT ? + ?";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_ERROR);

  SQLCHAR sqlState[6] = {'\0'};
  SQLINTEGER nativeError;
  SQLCHAR message[256];
  SQLSMALLINT messageLength;
  ret = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLength);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(sqlState), "07002");

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// ODBC lets an application bind parameters before SQLPrepare.
TEST_F(SQLPrepareTest, TestParametersBoundBeforePrepare) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLINTEGER custkeyParam = 42;
  ret                     = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_SLONG,
                         SQL_INTEGER,
                         0,
                         0,
                         &custkeyParam,
                         0,
                         NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = "SELECT custkey FROM tpch.sf1.customer WHERE custkey = ?";
  ret               = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLSMALLINT paramCount = 0;
  ret                    = SQLNumParams(hStmt, &paramCount);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(paramCount, 1);

  ret = SQLExecute(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLBIGINT result = 0;
  ret = SQLGetData(hStmt, 1, SQL_C_SBIGINT, &result, sizeof(result), NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(result, 42);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// The usual loop of prepare once, then execute, fetch and close for each
// value. Closing the cursor keeps the bound parameter and the described
// columns. The parameter is text bound as SQL_BIGINT, which has to reach
// Trino as a bigint to be compared with custkey.
TEST_F(SQLPrepareTest, TestExecuteAgainAfterClosingCursor) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = "SELECT custkey FROM tpch.sf1.customer WHERE custkey = ?";
  ret               = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  char custkeyParam[16] = {'\0'};
  ret                   = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_CHAR,
                         SQL_BIGINT,
                         0,
                         0,
                         custkeyParam,
                         sizeof(custkeyParam),
                         NULL);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  for (int custkey : {42, 7}) {
    strcpy_s(
        custkeyParam, sizeof(custkeyParam), std::to_string(custkey).c_str());
    ret = SQLExecute(hStmt);
    maybeReportStatementError(ret);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLFetch(hStmt);
    maybeReportStatementError(ret);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLBIGINT result = 0;
    ret = SQLGetData(hStmt, 1, SQL_C_SBIGINT, &result, sizeof(result), NULL);
    maybeReportStatementError(ret);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_EQ(result, custkey);

    ret = SQLFreeStmt(hStmt, SQL_CLOSE);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLSMALLINT columnCount = 0;
    ret                     = SQLNumResultCols(hStmt, &columnCount);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_EQ(columnCount, 1);
  }

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// A prepared statement holds its described columns, but no result
// until SQLExecute runs it, and none again once its cursor is closed.
// Fetching then is a function sequence error, not an empty result.
TEST_F(SQLPrepareTest, TestFetchBeforeExecuteAndAfterCloseFails) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = "SELECT 1";
  ret               = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLCHAR sqlState[SQL_SQLSTATE_SIZE + 1] = {0};
  SQLINTEGER nativeError                  = 0;
  ret                                     = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_ERROR);
  ret = SQLGetDiagRec(
      SQL_HANDLE_STMT, hStmt, 1, sqlState, &nativeError, NULL, 0, NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(sqlState), "HY010");

  ret = SQLExecute(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  ret = SQLFreeStmt(hStmt, SQL_CLOSE);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_ERROR);
  ret = SQLGetDiagRec(
      SQL_HANDLE_STMT, hStmt, 1, sqlState, &nativeError, NULL, 0, NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(sqlState), "HY010");

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// Closing the cursor forgets the closed result's column metadata, so a
// decimal's scale isn't reported for the next query's varchar column.
TEST_F(SQLPrepareTest, TestCloseClearsColumnMetadata) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string decimalQuery = "SELECT CAST(1.25 AS decimal(10, 2))";
  ret = SQLExecDirect(hStmt, (SQLCHAR*)decimalQuery.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLSMALLINT decimalDigits = -1;
  ret =
      SQLDescribeCol(hStmt, 1, NULL, 0, NULL, NULL, NULL, &decimalDigits, NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(decimalDigits, 2);

  ret = SQLFreeStmt(hStmt, SQL_CLOSE);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string varcharQuery = "SELECT 'abc'";
  ret = SQLExecDirect(hStmt, (SQLCHAR*)varcharQuery.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  decimalDigits = -1;
  ret =
      SQLDescribeCol(hStmt, 1, NULL, 0, NULL, NULL, NULL, &decimalDigits, NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(decimalDigits, 0);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// Nothing in this test reaches Trino, so it runs without a server.
TEST_F(SQLPrepareTest, TestNumParamsFollowsTheLatestStatement) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Before any statement there is nothing to count.
  SQLSMALLINT paramCount = -1;
  ret                    = SQLNumParams(hStmt, &paramCount);
  ASSERT_EQ(ret, SQL_ERROR);
  SQLCHAR sqlState[6] = {'\0'};
  SQLINTEGER nativeError;
  SQLCHAR message[256];
  SQLSMALLINT messageLength;
  ret = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLength);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(sqlState), "HY010");

  // SQLExecute with nothing prepared is a sequence error too.
  ret = SQLExecute(hStmt);
  ASSERT_EQ(ret, SQL_ERROR);
  ret = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLength);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(sqlState), "HY010");

  // Binding one parameter to a two marker query fails in the driver,
  // before anything is posted, but the statement is still the latest.
  SQLINTEGER value = 1;
  ret              = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_SLONG,
                         SQL_INTEGER,
                         0,
                         0,
                         &value,
                         0,
                         NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  std::string query = "SELECT ?, ?";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_ERROR);
  ret = SQLNumParams(hStmt, &paramCount);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(paramCount, 2);

  // SQLExecDirect replaced any prepared statement, so SQLExecute
  // must not run an older one.
  ret = SQLExecute(hStmt);
  ASSERT_EQ(ret, SQL_ERROR);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// A failure the driver finds itself must not repeat the Trino error
// from an earlier query on the same handle.
TEST_F(SQLPrepareTest, TestDriverErrorDoesNotRepeatEarlierTrinoError) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string badQuery = "SELECT * FROM tpch.sf1.no_such_table";
  ret = SQLExecDirect(hStmt, (SQLCHAR*)badQuery.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_ERROR);

  SQLINTEGER value = 1;
  ret              = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_SLONG,
                         SQL_INTEGER,
                         0,
                         0,
                         &value,
                         0,
                         NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  std::string query = "SELECT ? + ?";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_ERROR);

  SQLCHAR sqlState[6] = {'\0'};
  SQLINTEGER nativeError;
  SQLCHAR message[256];
  SQLSMALLINT messageLength;
  ret = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLength);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_STREQ(reinterpret_cast<char*>(sqlState), "07002");

  // The earlier missing table error is gone.
  ret = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      2,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLength);
  ASSERT_EQ(ret, SQL_NO_DATA);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// A PREPARE statement's markers are filled by a later EXECUTE, so
// parameters left bound from earlier work must not be applied to it.
TEST_F(SQLPrepareTest, TestExecDirectPrepareIgnoresBoundParameters) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLINTEGER leftover = 7;
  ret                 = SQLBindParameter(hStmt,
                         1,
                         SQL_PARAM_INPUT,
                         SQL_C_SLONG,
                         SQL_INTEGER,
                         0,
                         0,
                         &leftover,
                         0,
                         NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string prepare = "PREPARE bound_add_one FROM SELECT ? + 1";
  ret = SQLExecDirect(hStmt, (SQLCHAR*)prepare.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  while (SQLFetch(hStmt) == SQL_SUCCESS) {
  }

  std::string execute = "EXECUTE bound_add_one USING 41";
  ret = SQLExecDirect(hStmt, (SQLCHAR*)execute.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  SQLBIGINT result = 0;
  ret = SQLGetData(hStmt, 1, SQL_C_SBIGINT, &result, sizeof(result), NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(result, 42);
  while (SQLFetch(hStmt) == SQL_SUCCESS) {
  }

  std::string deallocate = "DEALLOCATE PREPARE bound_add_one";
  ret = SQLExecDirect(hStmt, (SQLCHAR*)deallocate.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);
  while (SQLFetch(hStmt) == SQL_SUCCESS) {
  }

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// A Trino error line longer than the whole buffer is truncated, and
// that must be reported rather than passed off as the full message.
TEST_F(SQLPrepareTest, TestTruncatedTrinoDiagnosticIsReported) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string badQuery = "SELECT * FROM tpch.sf1.no_such_table";
  ret = SQLExecDirect(hStmt, (SQLCHAR*)badQuery.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_ERROR);

  SQLCHAR sqlState[6] = {'\0'};
  SQLINTEGER nativeError;
  SQLCHAR message[8];
  SQLSMALLINT messageLength = 0;
  ret                       = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLength);
  ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
  // The length is that of the whole record, not of what fit.
  ASSERT_GT(messageLength, static_cast<SQLSMALLINT>(sizeof(message) - 1));
  ASSERT_EQ(strlen(reinterpret_cast<char*>(message)), sizeof(message) - 1);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}
