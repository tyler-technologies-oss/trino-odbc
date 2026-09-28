#include <windows.h>

#include <sql.h>
#include <sqlext.h>

#include <gtest/gtest.h>

#include "../fixtures/sqlDriverConnectFixture.hpp"

class GetStmtAttrTest : public SQLDriverConnectFixture {};

TEST_F(GetStmtAttrTest, UnknownAttributeErrorIsReadable) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // Not a statement attribute ODBC defines, so the driver rejects it.
  SQLUINTEGER value = 0;
  ret = SQLGetStmtAttr(hStmt, 65000, &value, SQL_IS_UINTEGER, nullptr);
  ASSERT_EQ(ret, SQL_ERROR);

  // The error the driver recorded on the statement must be what the
  // application reads back, not SQL_NO_DATA.
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
  EXPECT_STREQ(reinterpret_cast<const char*>(sqlState), "HY092");

  // A later call that succeeds leaves nothing behind to report.
  SQLULEN rowNumber = 0;
  ret = SQLGetStmtAttr(hStmt, SQL_ATTR_ROW_NUMBER, &rowNumber, 0, nullptr);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLGetDiagRec(SQL_HANDLE_STMT,
                      hStmt,
                      1,
                      sqlState,
                      &nativeError,
                      message,
                      sizeof(message),
                      &messageLen);
  EXPECT_EQ(ret, SQL_NO_DATA);

  ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
}

TEST_F(GetStmtAttrTest, CursorAttributesDescribeAForwardOnlyCursor) {
  // MSDASQL, which SQL Server linked servers use, reads the cursor
  // type before it fetches, and stops if it can't.
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLULEN cursorType = 99;
  ret = SQLGetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, &cursorType, 0, nullptr);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(cursorType, SQL_CURSOR_FORWARD_ONLY);

  SQLULEN concurrency = 99;
  ret = SQLGetStmtAttr(hStmt, SQL_ATTR_CONCURRENCY, &concurrency, 0, nullptr);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(concurrency, SQL_CONCUR_READ_ONLY);

  // Asking for more than one row per fetch keeps the one row, and
  // says so with 01S02 rather than failing.
  ret = SQLSetStmtAttr(
      hStmt, SQL_ATTR_ROW_ARRAY_SIZE, reinterpret_cast<SQLPOINTER>(10), 0);
  ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
  SQLCHAR sqlState[SQL_SQLSTATE_SIZE + 1] = {0};
  SQLINTEGER nativeError                  = 0;
  ret                                     = SQLGetDiagRec(
      SQL_HANDLE_STMT, hStmt, 1, sqlState, &nativeError, nullptr, 0, nullptr);
  EXPECT_STREQ(reinterpret_cast<const char*>(sqlState), "01S02");

  SQLULEN arraySize = 0;
  ret = SQLGetStmtAttr(hStmt, SQL_ATTR_ROW_ARRAY_SIZE, &arraySize, 0, nullptr);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(arraySize, 1);
}

TEST_F(GetStmtAttrTest, FetchScrollReportsRowsFetchedAndStatus) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);

  SQLULEN rowsFetched    = 99;
  SQLUSMALLINT rowStatus = 99;
  ret = SQLSetStmtAttr(hStmt, SQL_ATTR_ROWS_FETCHED_PTR, &rowsFetched, 0);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLSetStmtAttr(hStmt, SQL_ATTR_ROW_STATUS_PTR, &rowStatus, 0);
  ASSERT_EQ(ret, SQL_SUCCESS);

  // MSDASQL binds each column once, then moves the buffers along with
  // SQL_ATTR_ROW_BIND_OFFSET_PTR. Bind at the start of a two-slot
  // buffer and fetch into the second slot.
  SQLINTEGER values[2] = {0, 0};
  SQLLEN bindOffset    = sizeof(SQLINTEGER);
  ret = SQLBindCol(hStmt, 1, SQL_C_SLONG, &values[0], sizeof(values[0]), 0);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLSetStmtAttr(hStmt, SQL_ATTR_ROW_BIND_OFFSET_PTR, &bindOffset, 0);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::string query = "SELECT 42";
  ret               = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);

  ret = SQLFetchScroll(hStmt, SQL_FETCH_NEXT, 0);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(rowsFetched, 1);
  EXPECT_EQ(rowStatus, SQL_ROW_SUCCESS);
  EXPECT_EQ(values[0], 0);
  EXPECT_EQ(values[1], 42);

  ret = SQLFetchScroll(hStmt, SQL_FETCH_NEXT, 0);
  ASSERT_EQ(ret, SQL_NO_DATA);
  EXPECT_EQ(rowsFetched, 0);

  // A forward-only cursor can't go back.
  ret = SQLFetchScroll(hStmt, SQL_FETCH_FIRST, 0);
  ASSERT_EQ(ret, SQL_ERROR);

  // After SQL_UNBIND, fetching writes nothing to the old buffers.
  ret = SQLFreeStmt(hStmt, SQL_CLOSE);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFreeStmt(hStmt, SQL_UNBIND);
  ASSERT_EQ(ret, SQL_SUCCESS);
  values[1] = 0;
  ret       = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(values[1], 0);
}
