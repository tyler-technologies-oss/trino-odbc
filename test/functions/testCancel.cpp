#include <windows.h>

#include <gtest/gtest.h>
#include <iostream>
#include <sql.h>
#include <sqlext.h>

#include "../constants.hpp"

#include "../fixtures/sqlDriverConnectFixture.hpp"

class SQLCancelTest : public SQLDriverConnectFixture {};

TEST_F(SQLCancelTest, TestCancelQuery) {
  // Allocate statement handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Execute the query, one that will take a few seconds
  // to fully return. It can be silly.
  std::string query = R"SQL(
      SELECT *
      FROM tpch.sf100.customer AS c1
      JOIN tpch.sf100.customer AS c2
          ON c1.custkey = c2.custkey
      WHERE (c1.custkey + c2.custkey) % 7 = 0
  )SQL";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Bind the column to a bigint variable
  SQLSMALLINT cType   = SQL_C_SBIGINT;
  SQLBIGINT result    = 0;
  SQLLEN indicator    = 0;
  SQLLEN bufferLength = -1; // Ignored for integer types.
  ret = SQLBindCol(hStmt, 1, cType, &result, bufferLength, &indicator);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Fetch a tiny bit of the data
  ret = SQLFetch(hStmt);
  ASSERT_TRUE(ret == SQL_SUCCESS or ret == SQL_NO_DATA);

  // Now cancel the query
  ret = SQLCancel(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Free statement handle
  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

// Cancelling sends Trino a DELETE on the connection's shared curl
// handle. The next query on the connection must still go out as a
// POST, or Trino rejects it with 405 Method Not Allowed.
TEST_F(SQLCancelTest, TestQueryAfterCancelOnSameConnection) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = "SELECT custkey FROM tpch.sf1.customer";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLCancel(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // A fresh statement on the same connection.
  ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  std::string nextQuery = "SELECT 2";
  ret = SQLExecDirect(hStmt, (SQLCHAR*)nextQuery.c_str(), SQL_NTS);
  maybeReportStatementError(ret);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  SQLBIGINT result = 0;
  ret = SQLGetData(hStmt, 1, SQL_C_SBIGINT, &result, sizeof(result), NULL);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ASSERT_EQ(result, 2);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}
