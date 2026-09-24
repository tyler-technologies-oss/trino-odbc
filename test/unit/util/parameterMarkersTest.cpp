#include <gtest/gtest.h>
#include <string>

#include "../../../src/util/parameterMarkers.hpp"

TEST(ParameterMarkersTest, NoMarkers) {
  EXPECT_EQ(countParameterMarkers("SELECT 1"), 0);
}

TEST(ParameterMarkersTest, EmptyQuery) {
  EXPECT_EQ(countParameterMarkers(""), 0);
}

TEST(ParameterMarkersTest, MarkersInExpressions) {
  std::string query = "SELECT * FROM t WHERE ts BETWEEN "
                      "cast(? as timestamp) AND cast(? as timestamp)";
  EXPECT_EQ(countParameterMarkers(query), 2);
}

TEST(ParameterMarkersTest, MarkersWithoutSpaces) {
  EXPECT_EQ(countParameterMarkers("SELECT ?+?,?"), 3);
}

TEST(ParameterMarkersTest, MarkerInStringLiteralIsIgnored) {
  EXPECT_EQ(countParameterMarkers("SELECT 'why?' WHERE a = ?"), 1);
}

TEST(ParameterMarkersTest, EscapedQuoteDoesNotEndStringLiteral) {
  EXPECT_EQ(countParameterMarkers("SELECT 'it''s ?' WHERE a = ?"), 1);
}

TEST(ParameterMarkersTest, MarkerInQuotedIdentifierIsIgnored) {
  EXPECT_EQ(countParameterMarkers(R"(SELECT "odd?""name" FROM t WHERE a = ?)"),
            1);
}

TEST(ParameterMarkersTest, MarkerInLineCommentIsIgnored) {
  EXPECT_EQ(countParameterMarkers("SELECT ? -- is this right?\nFROM t"), 1);
}

TEST(ParameterMarkersTest, MarkerInBlockCommentIsIgnored) {
  EXPECT_EQ(countParameterMarkers("SELECT /* ? */ ? FROM t"), 1);
}

TEST(ParameterMarkersTest, UnterminatedStringLiteral) {
  EXPECT_EQ(countParameterMarkers("SELECT ?, 'no end ?"), 1);
}

TEST(ParameterMarkersTest, UnterminatedBlockComment) {
  EXPECT_EQ(countParameterMarkers("SELECT ? /* no end ?"), 1);
}
