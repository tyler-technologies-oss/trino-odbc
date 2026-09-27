#include <windows.h>

#include <gtest/gtest.h>
#include <sql.h>
#include <sqlext.h>
#include <string>

#include "../fixtures/sqlDriverConnectFixture.hpp"

// Connects with UnboundedVarchar=LongVarchar, as a SQL Server linked
// server's connection string can.
class UnboundedVarcharTest : public SQLDriverConnectFixture {
  protected:
    void SetUp() override {
      return SQLDriverConnectFixture::SetUp("UnboundedVarchar=LongVarchar;");
    }
};

// The same queries with the setting left alone.
class UnboundedVarcharDefaultTest : public SQLDriverConnectFixture {};

static void
describeFirstColumn(SQLHSTMT hStmt, SQLSMALLINT* dataType, SQLULEN* colSize) {
  SQLCHAR colName[128];
  SQLSMALLINT nameLen       = 0;
  SQLSMALLINT decimalDigits = 0;
  SQLSMALLINT nullable      = 0;
  SQLRETURN ret             = SQLDescribeCol(hStmt,
                                 1,
                                 colName,
                                 sizeof(colName),
                                 &nameLen,
                                 dataType,
                                 colSize,
                                 &decimalDigits,
                                 &nullable);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(UnboundedVarcharTest, UnboundedVarcharIsLongText) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // MSDASQL prepares its queries, so describe a prepared one.
  std::string query = "SELECT CAST(name AS varchar) AS name "
                      "FROM tpch.sf1.nation ORDER BY nationkey";
  ret               = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLSMALLINT dataType = 0;
  SQLULEN colSize      = 0;
  describeFirstColumn(hStmt, &dataType, &colSize);
  EXPECT_EQ(dataType, SQL_LONGVARCHAR);
  EXPECT_EQ(colSize, 2147483647);

  SQLLEN conciseType = 0;
  ret                = SQLColAttribute(
      hStmt, 1, SQL_DESC_CONCISE_TYPE, nullptr, 0, nullptr, &conciseType);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(conciseType, SQL_LONGVARCHAR);

  // The values are still ordinary text to read.
  ret = SQLExecute(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLCHAR buffer[64] = {0};
  SQLLEN indicator   = 0;
  ret = SQLGetData(hStmt, 1, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
  ASSERT_EQ(ret, SQL_SUCCESS);
  std::string value(reinterpret_cast<char*>(buffer));
  EXPECT_EQ(value, "ALGERIA");

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(UnboundedVarcharTest, BoundedVarcharStaysVarchar) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // nation.name is a varchar(25).
  std::string query = "SELECT name FROM tpch.sf1.nation";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLSMALLINT dataType = 0;
  SQLULEN colSize      = 0;
  describeFirstColumn(hStmt, &dataType, &colSize);
  EXPECT_EQ(dataType, SQL_VARCHAR);
  EXPECT_EQ(colSize, 25);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(UnboundedVarcharTest, ColumnsReportsLongText) {
  // SQL Server reads column types with SQLColumns for four-part names,
  // so it has to agree with SQLDescribeCol. system.runtime.nodes has
  // unbounded varchars in every Trino.
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  ret = SQLColumns(hStmt,
                   (SQLCHAR*)"system",
                   SQL_NTS,
                   (SQLCHAR*)"runtime",
                   SQL_NTS,
                   (SQLCHAR*)"nodes",
                   SQL_NTS,
                   (SQLCHAR*)"node_id",
                   SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // DATA_TYPE is column 5, and SQL_DATA_TYPE is column 14.
  SQLSMALLINT dataType    = 0;
  SQLSMALLINT sqlDataType = 0;
  SQLLEN indicator        = 0;
  ret = SQLGetData(hStmt, 5, SQL_C_SSHORT, &dataType, 0, &indicator);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(dataType, SQL_LONGVARCHAR);
  ret = SQLGetData(hStmt, 14, SQL_C_SSHORT, &sqlDataType, 0, &indicator);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(sqlDataType, SQL_LONGVARCHAR);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(UnboundedVarcharDefaultTest, UnboundedVarcharIsVarcharByDefault) {
  // Excel and Power BI see what they always have.
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = "SELECT CAST(name AS varchar) AS name "
                      "FROM tpch.sf1.nation";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLSMALLINT dataType = 0;
  SQLULEN colSize      = 0;
  describeFirstColumn(hStmt, &dataType, &colSize);
  EXPECT_EQ(dataType, SQL_VARCHAR);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}
