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

TEST(ParameterMarkersTest, PrepareStatementIsDetected) {
  EXPECT_TRUE(isPrepareStatement("PREPARE p FROM SELECT ? + 1"));
}

TEST(ParameterMarkersTest, PrepareKeywordIgnoresCase) {
  EXPECT_TRUE(isPrepareStatement("prepare p from select ?"));
}

TEST(ParameterMarkersTest, PrepareAfterWhitespaceAndComments) {
  EXPECT_TRUE(
      isPrepareStatement("\n  -- note\n /* block */ Prepare p FROM SELECT ?"));
}

TEST(ParameterMarkersTest, PrepareMustBeTheWholeWord) {
  EXPECT_FALSE(isPrepareStatement("PREPARED_TABLE"));
  EXPECT_FALSE(isPrepareStatement("PREPARE_ME"));
}

TEST(ParameterMarkersTest, OtherStatementsAreNotPrepare) {
  EXPECT_FALSE(isPrepareStatement("SELECT 'PREPARE' WHERE a = ?"));
  EXPECT_FALSE(isPrepareStatement("EXECUTE p USING ?"));
  EXPECT_FALSE(isPrepareStatement(""));
  EXPECT_FALSE(isPrepareStatement("PREP"));
}

TEST(ParameterMarkersTest, PrepareStatementHasNoParametersToBind) {
  EXPECT_EQ(countParametersToBind("PREPARE p FROM SELECT ? + ?"), 0);
  EXPECT_EQ(countParametersToBind("SELECT ? + ?"), 2);
  EXPECT_EQ(countParametersToBind("EXECUTE p USING ?"), 1);
}
