#include <windows.h>

#include <gtest/gtest.h>
#include <iostream>
#include <sql.h>
#include <sqlext.h>

#include "../constants.hpp"

#include "../fixtures/sqlDriverConnectFixture.hpp"

class SQLDescribColTest : public SQLDriverConnectFixture {};

void setupTpchCustomerQuery(SQLHSTMT hStmt) {
  std::string query = R"SQL(
      SELECT *
      FROM tpch.sf1.customer
  )SQL";
  SQLRETURN ret     = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

void setupTpchTinyintQuery(SQLHSTMT hStmt) {
  /*
  tpch doesn't contain any TINYINTs, so we can create one
  manually on this very small nation table to enable testing
  that type
  */
  std::string query = R"SQL(
      SELECT
        CAST(nationkey AS TINYINT) AS nationkey,
        name,
        regionkey,
        comment
      FROM tpch.sf1.nation
  )SQL";
  SQLRETURN ret     = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLDescribColTest, TestDescribeBigintCol) {
  // Allocate statement handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Run the query
  setupTpchCustomerQuery(hStmt);

  // Prepare buffers for SQLDescribeCol
  SQLCHAR colName[128];
  SQLSMALLINT nameLen       = 0;
  SQLSMALLINT dataType      = 0;
  SQLULEN colSize           = 0;
  SQLSMALLINT decimalDigits = 0;
  SQLSMALLINT nullable      = 0;

  // Describe the first column (custkey)
  ret = SQLDescribeCol(hStmt,
                       1, // Column number (1-based)
                       colName,
                       sizeof(colName),
                       &nameLen,
                       &dataType,
                       &colSize,
                       &decimalDigits,
                       &nullable);
  ASSERT_EQ(ret, SQL_SUCCESS);

  EXPECT_STREQ((const char*)colName, "custkey");
  EXPECT_EQ(dataType, SQL_BIGINT);

  // Free statement handle
  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLDescribColTest, TestDescribeVarcharCol) {
  // Allocate statement handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Run the query
  setupTpchCustomerQuery(hStmt);

  // Prepare buffers for SQLDescribeCol
  SQLCHAR colName[128];
  SQLSMALLINT nameLen       = 0;
  SQLSMALLINT dataType      = 0;
  SQLULEN colSize           = 0;
  SQLSMALLINT decimalDigits = 0;
  SQLSMALLINT nullable      = 0;

  // Describe the second column (name)
  ret = SQLDescribeCol(hStmt,
                       2, // Column number (1-based)
                       colName,
                       sizeof(colName),
                       &nameLen,
                       &dataType,
                       &colSize,
                       &decimalDigits,
                       &nullable);
  ASSERT_EQ(ret, SQL_SUCCESS);

  EXPECT_STREQ((const char*)colName, "name");
  EXPECT_EQ(dataType, SQL_VARCHAR);

  // Free statement handle
  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}


TEST_F(SQLDescribColTest, TestDescribeDoubleColumn) {
  // Allocate statement handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Run the query
  setupTpchCustomerQuery(hStmt);

  // Prepare buffers for SQLDescribeCol
  SQLCHAR colName[128];
  SQLSMALLINT nameLen       = 0;
  SQLSMALLINT dataType      = 0;
  SQLULEN colSize           = 0;
  SQLSMALLINT decimalDigits = 0;
  SQLSMALLINT nullable      = 0;

  // Describe the sixth column (acctbal)
  ret = SQLDescribeCol(hStmt,
                       6, // Column number (1-based)
                       colName,
                       sizeof(colName),
                       &nameLen,
                       &dataType,
                       &colSize,
                       &decimalDigits,
                       &nullable);
  ASSERT_EQ(ret, SQL_SUCCESS);

  EXPECT_STREQ((const char*)colName, "acctbal");
  EXPECT_EQ(dataType, SQL_DOUBLE);

  // Free statement handle
  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLDescribColTest, TestDescribeTinyintColumn) {
  // Allocate statement handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Run the query
  setupTpchTinyintQuery(hStmt);

  // Prepare buffers for SQLDescribeCol
  SQLCHAR colName[128];
  SQLSMALLINT nameLen       = 0;
  SQLSMALLINT dataType      = 0;
  SQLULEN colSize           = 0;
  SQLSMALLINT decimalDigits = 0;
  SQLSMALLINT nullable      = 0;

  // Describe the first column (nationkey)
  ret = SQLDescribeCol(hStmt,
                       1, // Column number (1-based)
                       colName,
                       sizeof(colName),
                       &nameLen,
                       &dataType,
                       &colSize,
                       &decimalDigits,
                       &nullable);
  ASSERT_EQ(ret, SQL_SUCCESS);

  EXPECT_STREQ((const char*)colName, "nationkey");
  EXPECT_EQ(dataType, SQL_TINYINT);

  // Free statement handle
  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLDescribColTest, TestColAttributeTruncationIsReadable) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  setupTpchCustomerQuery(hStmt);

  // Too small for "custkey" and its null termination character.
  SQLCHAR colName[4];
  SQLSMALLINT nameLen = 0;
  ret                 = SQLColAttribute(
      hStmt, 1, SQL_DESC_NAME, colName, sizeof(colName), &nameLen, nullptr);
  ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
  EXPECT_EQ(nameLen, 7);
  EXPECT_STREQ(reinterpret_cast<const char*>(colName), "cus");

  // The warning behind SQL_SUCCESS_WITH_INFO must be the first
  // diagnostic record on the statement.
  SQLCHAR sqlState[SQL_SQLSTATE_SIZE + 1] = {0};
  SQLCHAR message[256]                    = {0};
  SQLINTEGER nativeError                  = 0;
  SQLSMALLINT messageLen                  = 0;

  ret = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLen);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_STREQ(reinterpret_cast<const char*>(sqlState), "01004");

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLDescribColTest, TestColAttributeWritesWholeSQLLEN) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // BIGINT and BIT have negative type codes, so they only read back
  // correctly if the driver writes every byte of the SQLLEN.
  std::string query = "SELECT CAST(1 AS BIGINT) AS b, true AS f";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLLEN bigintType = 0;
  ret               = SQLColAttribute(
      hStmt, 1, SQL_DESC_CONCISE_TYPE, nullptr, 0, nullptr, &bigintType);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(bigintType, SQL_BIGINT);

  SQLLEN booleanType = 0;
  ret                = SQLColAttribute(
      hStmt, 2, SQL_DESC_CONCISE_TYPE, nullptr, 0, nullptr, &booleanType);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(booleanType, SQL_BIT);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(SQLDescribColTest, TestExecDirectReportsRejectedQuery) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Trino only rejects this after the POST, on a later poll.
  std::string query = "SELECT * FROM tpch.sf1.no_such_table";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_ERROR);

  // The Trino error must be readable as a diagnostic record.
  SQLCHAR sqlState[SQL_SQLSTATE_SIZE + 1] = {0};
  SQLCHAR message[1024]                   = {0};
  SQLINTEGER nativeError                  = 0;
  SQLSMALLINT messageLen                  = 0;

  ret = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLen);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_GT(messageLen, 0);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}
