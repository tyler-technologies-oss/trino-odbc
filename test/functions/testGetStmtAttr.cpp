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
  ret                                     = SQLGetDiagRec(SQL_HANDLE_STMT,
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
