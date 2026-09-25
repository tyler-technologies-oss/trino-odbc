#include <windows.h>

#include <gtest/gtest.h>
#include <sql.h>
#include <sqlext.h>
#include <string>

#include "../../src/util/unicodeConversion.hpp"
#include "../constants.hpp"
#include "../fixtures/sqlDriverConnectFixture.hpp"

/*
Text through the Unicode (W) functions and SQL_C_WCHAR buffers. Trino
works in UTF-8, and Windows applications such as .NET work in UTF-16,
so non-ASCII text has to be converted both ways.

The queries build non-ASCII strings with chr() so this file stays
ASCII. chr(233) is é, and chr(128512) is 😀, which needs a UTF-16
surrogate pair.

Other string literals are ASCII too, with "é" written as UTF-8 bytes
and converted. MSVC reads a source file without a byte order mark in
the ANSI code page, so a UTF-8 "é" in a literal would compile as two
characters.
*/

// "café" as UTF-8, and then as UTF-16.
static const std::string CAFE_UTF8     = "caf\xC3\xA9";
static const std::u16string CAFE_UTF16 = utf8ToUtf16(CAFE_UTF8);
class UnicodeTest : public SQLDriverConnectFixture {
  protected:
    void execDirectW(const std::u16string& query) {
      SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
      ASSERT_EQ(ret, SQL_SUCCESS);
      ret = SQLExecDirectW(
          hStmt,
          reinterpret_cast<SQLWCHAR*>(const_cast<char16_t*>(query.c_str())),
          SQL_NTS);
      this->maybeReportStatementError(ret);
      ASSERT_EQ(ret, SQL_SUCCESS);
      ret = SQLFetch(hStmt);
      ASSERT_EQ(ret, SQL_SUCCESS);
    }

    void TearDown() override {
      if (hStmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
      }
      SQLDriverConnectFixture::TearDown();
    }
};

TEST_F(UnicodeTest, GetDataAsWideChar) {
  execDirectW(u"SELECT 'caf' || chr(233)");

  char16_t result[16] = {};
  SQLLEN indicator    = 0;
  SQLRETURN ret =
      SQLGetData(hStmt, 1, SQL_C_WCHAR, result, sizeof(result), &indicator);

  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(result), CAFE_UTF16);
  // The length of a SQL_C_WCHAR value is in bytes.
  EXPECT_EQ(indicator, 4 * sizeof(char16_t));
}

TEST_F(UnicodeTest, GetDataAsWideCharAboveBmp) {
  execDirectW(u"SELECT 'x' || chr(128512)");

  char16_t result[16] = {};
  SQLLEN indicator    = 0;
  SQLRETURN ret =
      SQLGetData(hStmt, 1, SQL_C_WCHAR, result, sizeof(result), &indicator);

  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(result), (std::u16string{u'x', 0xD83D, 0xDE00}));
  EXPECT_EQ(indicator, 3 * sizeof(char16_t));
}

TEST_F(UnicodeTest, GetDataAsCharStillReturnsUtf8) {
  // Applications that ask for SQL_C_CHAR get Trino's UTF-8 bytes, as
  // they always have.
  execDirectW(u"SELECT 'caf' || chr(233)");

  char result[16]  = {};
  SQLLEN indicator = 0;
  SQLRETURN ret =
      SQLGetData(hStmt, 1, SQL_C_CHAR, result, sizeof(result), &indicator);

  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_STREQ(result, "caf\xC3\xA9");
  EXPECT_EQ(indicator, 5);
}

TEST_F(UnicodeTest, GetDataAsWideCharReportsTruncation) {
  execDirectW(u"SELECT 'caf' || chr(233)");

  // Room for two characters and a terminator, with guard characters
  // after it.
  char16_t result[6] = {u'x', u'x', u'x', u'x', u'x', u'x'};
  SQLLEN indicator   = 0;
  SQLRETURN ret      = SQLGetData(
      hStmt, 1, SQL_C_WCHAR, result, 3 * sizeof(char16_t), &indicator);

  EXPECT_EQ(ret, SQL_SUCCESS_WITH_INFO);
  EXPECT_EQ(std::u16string(result), u"ca");
  EXPECT_EQ(result[3], u'x');
  // The length is the whole value's, so the application can ask again.
  EXPECT_EQ(indicator, 4 * sizeof(char16_t));

  SQLCHAR sqlState[6] = {};
  SQLINTEGER nativeError;
  SQLCHAR message[256];
  SQLSMALLINT messageLength;
  SQLGetDiagRec(SQL_HANDLE_STMT,
                hStmt,
                1,
                sqlState,
                &nativeError,
                message,
                sizeof(message),
                &messageLength);
  EXPECT_STREQ(reinterpret_cast<char*>(sqlState), "01004");
}

TEST_F(UnicodeTest, ExactFitWideCharIsNotTruncated) {
  execDirectW(u"SELECT 'caf' || chr(233)");

  // Four characters and a terminator, with nothing to spare.
  char16_t result[5] = {};
  SQLLEN indicator   = 0;
  SQLRETURN ret =
      SQLGetData(hStmt, 1, SQL_C_WCHAR, result, sizeof(result), &indicator);

  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(result), CAFE_UTF16);
}

TEST_F(UnicodeTest, BindColAsWideChar) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  std::u16string query = u"SELECT 'caf' || chr(233)";
  ret                  = SQLExecDirectW(
      hStmt,
      reinterpret_cast<SQLWCHAR*>(const_cast<char16_t*>(query.c_str())),
      SQL_NTS);
  ASSERT_EQ(ret, SQL_SUCCESS);

  char16_t result[16] = {};
  SQLLEN indicator    = 0;
  ret = SQLBindCol(hStmt, 1, SQL_C_WCHAR, result, sizeof(result), &indicator);
  ASSERT_EQ(ret, SQL_SUCCESS);

  ret = SQLFetch(hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(result), CAFE_UTF16);
  EXPECT_EQ(indicator, 4 * sizeof(char16_t));
}

TEST_F(UnicodeTest, WideQueryTextKeepsNonAsciiLiterals) {
  // The query text itself holds é. Converted through the ANSI code
  // page it would reach Trino as a byte that isn't valid UTF-8.
  execDirectW(utf8ToUtf16("SELECT length('" + CAFE_UTF8 + "'), '" + CAFE_UTF8 +
                          "' = 'caf' || chr(233)"));

  SQLINTEGER length = 0;
  SQLLEN indicator  = 0;
  SQLRETURN ret =
      SQLGetData(hStmt, 1, SQL_C_SLONG, &length, sizeof(length), &indicator);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(length, 4);

  char isEqual[8] = {};
  ret = SQLGetData(hStmt, 2, SQL_C_CHAR, isEqual, sizeof(isEqual), &indicator);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_STREQ(isEqual, "1");
}

TEST_F(UnicodeTest, CharColumnIsDescribedAsChar) {
  // Trino's char(n) type used to be missing from the type mappings,
  // so it was described as type 0 and .NET refused to read it.
  execDirectW(u"SELECT CAST('ab' || chr(233) AS char(4))");

  SQLSMALLINT dataType = 0;
  SQLRETURN ret        = SQLDescribeCol(
      hStmt, 1, nullptr, 0, nullptr, &dataType, nullptr, nullptr, nullptr);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(dataType, SQL_CHAR);

  char16_t result[16] = {};
  SQLLEN indicator    = 0;
  ret = SQLGetData(hStmt, 1, SQL_C_WCHAR, result, sizeof(result), &indicator);
  ASSERT_EQ(ret, SQL_SUCCESS);
  // Trino pads a char(n) value with spaces to its full length.
  EXPECT_EQ(std::u16string(result), utf8ToUtf16("ab\xC3\xA9 "));
}

TEST_F(UnicodeTest, DescribeColWReturnsWideColumnName) {
  execDirectW(utf8ToUtf16("SELECT 1 AS \"" + CAFE_UTF8 + "\""));

  char16_t name[16]      = {};
  SQLSMALLINT nameLength = 0;
  SQLRETURN ret          = SQLDescribeColW(hStmt,
                                  1,
                                  reinterpret_cast<SQLWCHAR*>(name),
                                  16,
                                  &nameLength,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  nullptr);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(name), CAFE_UTF16);
  // SQLDescribeColW counts characters, not bytes.
  EXPECT_EQ(nameLength, 4);
}

TEST_F(UnicodeTest, ColAttributeWReturnsWideColumnName) {
  execDirectW(utf8ToUtf16("SELECT 1 AS \"" + CAFE_UTF8 + "\""));

  char16_t name[16]      = {};
  SQLSMALLINT nameLength = 0;
  SQLRETURN ret          = SQLColAttributeW(
      hStmt, 1, SQL_DESC_NAME, name, sizeof(name), &nameLength, nullptr);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(name), CAFE_UTF16);
  // SQLColAttributeW counts bytes, since the same buffer can hold a
  // number.
  EXPECT_EQ(nameLength, 4 * sizeof(char16_t));
}

TEST_F(UnicodeTest, GetDiagRecWReturnsWideMessage) {
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
  ASSERT_EQ(ret, SQL_SUCCESS);
  std::u16string query = u"SELECT * FROM tpch.sf1.no_such_table";
  ret                  = SQLExecDirectW(
      hStmt,
      reinterpret_cast<SQLWCHAR*>(const_cast<char16_t*>(query.c_str())),
      SQL_NTS);
  ASSERT_EQ(ret, SQL_ERROR);

  char16_t sqlState[6]      = {};
  SQLINTEGER nativeError    = 0;
  char16_t message[1024]    = {};
  SQLSMALLINT messageLength = 0;
  ret                       = SQLGetDiagRecW(SQL_HANDLE_STMT,
                       hStmt,
                       1,
                       reinterpret_cast<SQLWCHAR*>(sqlState),
                       &nativeError,
                       reinterpret_cast<SQLWCHAR*>(message),
                       1024,
                       &messageLength);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(std::u16string(sqlState).size(), 5);
  std::string messageText =
      utf16ToUtf8(message, std::u16string(message).size());
  EXPECT_NE(messageText.find("no_such_table"), std::string::npos)
      << messageText;
  EXPECT_EQ(messageLength, std::u16string(message).size());
}

TEST_F(UnicodeTest, GetInfoWReturnsWideString) {
  // Needs no server. SQLGetInfoW counts bytes.
  char16_t driverName[32]  = {};
  SQLSMALLINT stringLength = 0;
  SQLRETURN ret            = SQLGetInfoW(
      hDbc, SQL_DRIVER_NAME, driverName, sizeof(driverName), &stringLength);
  ASSERT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(driverName), u"TrinoODBC");
  EXPECT_EQ(stringLength, 9 * sizeof(char16_t));
}

TEST(UnicodeConnectTest, DriverConnectWConnectsAndReturnsConnectionString) {
  // Needs no server, since connecting doesn't send a query.
  SQLHENV hEnv  = nullptr;
  SQLHDBC hDbc  = nullptr;
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
  ASSERT_EQ(ret, SQL_SUCCESS);
  ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
  ASSERT_EQ(ret, SQL_SUCCESS);

  std::u16string connStr    = utf8ToUtf16("DSN=" + TEST_DSN + ";");
  char16_t outConnStr[1024] = {};
  SQLSMALLINT outLength     = 0;
  ret                       = SQLDriverConnectW(
      hDbc,
      nullptr,
      reinterpret_cast<SQLWCHAR*>(const_cast<char16_t*>(connStr.c_str())),
      SQL_NTS,
      reinterpret_cast<SQLWCHAR*>(outConnStr),
      1024,
      &outLength,
      SQL_DRIVER_NOPROMPT);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(outConnStr), connStr);
  EXPECT_EQ(outLength, connStr.size());

  SQLDisconnect(hDbc);
  SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
  SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
}

TEST_F(UnicodeTest, GetDataReturnsLongWideTextInParts) {
  execDirectW(u"SELECT 'caf' || chr(233)");

  // Room for two characters and a terminator.
  char16_t part[3] = {};
  SQLLEN indicator = 0;

  // The first part, and the length of the whole value.
  SQLRETURN ret =
      SQLGetData(hStmt, 1, SQL_C_WCHAR, part, sizeof(part), &indicator);
  EXPECT_EQ(ret, SQL_SUCCESS_WITH_INFO);
  EXPECT_EQ(std::u16string(part), u"ca");
  EXPECT_EQ(indicator, 4 * sizeof(char16_t));

  // The rest, and the length of what was left.
  ret = SQLGetData(hStmt, 1, SQL_C_WCHAR, part, sizeof(part), &indicator);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(part), (std::u16string{u'f', 0x00E9}));
  EXPECT_EQ(indicator, 2 * sizeof(char16_t));

  // Nothing is left.
  ret = SQLGetData(hStmt, 1, SQL_C_WCHAR, part, sizeof(part), &indicator);
  EXPECT_EQ(ret, SQL_NO_DATA);
}

TEST_F(UnicodeTest, GetDataReturnsLongCharTextInParts) {
  execDirectW(u"SELECT 'abcdefg'");

  char part[4]     = {};
  SQLLEN indicator = 0;
  std::string whole;
  SQLRETURN ret = SQL_SUCCESS_WITH_INFO;
  int calls     = 0;
  while (ret == SQL_SUCCESS_WITH_INFO and calls < 10) {
    ret = SQLGetData(hStmt, 1, SQL_C_CHAR, part, sizeof(part), &indicator);
    whole += part;
    calls++;
  }
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(whole, "abcdefg");
  EXPECT_EQ(calls, 3);
  EXPECT_EQ(SQLGetData(hStmt, 1, SQL_C_CHAR, part, sizeof(part), &indicator),
            SQL_NO_DATA);
}

TEST_F(UnicodeTest, GetDataPartsDontSplitSurrogatePairs) {
  // "x😀" is three UTF-16 code units. A part with room for two would
  // end in the middle of the emoji, so the first part is only "x".
  execDirectW(u"SELECT 'x' || chr(128512)");

  char16_t part[3] = {};
  SQLLEN indicator = 0;
  SQLRETURN ret =
      SQLGetData(hStmt, 1, SQL_C_WCHAR, part, sizeof(part), &indicator);
  EXPECT_EQ(ret, SQL_SUCCESS_WITH_INFO);
  EXPECT_EQ(std::u16string(part), u"x");

  ret = SQLGetData(hStmt, 1, SQL_C_WCHAR, part, sizeof(part), &indicator);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(part), (std::u16string{0xD83D, 0xDE00}));
}

TEST_F(UnicodeTest, GetDataLengthQueryDoesNotConsumeTheValue) {
  execDirectW(u"SELECT 'caf' || chr(233)");

  // A zero length buffer asks for the length without reading anything.
  SQLLEN indicator = 0;
  SQLRETURN ret    = SQLGetData(hStmt, 1, SQL_C_WCHAR, nullptr, 0, &indicator);
  EXPECT_EQ(ret, SQL_SUCCESS_WITH_INFO);
  EXPECT_EQ(indicator, 4 * sizeof(char16_t));

  char16_t result[16] = {};
  ret = SQLGetData(hStmt, 1, SQL_C_WCHAR, result, sizeof(result), &indicator);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(std::u16string(result), CAFE_UTF16);
}
