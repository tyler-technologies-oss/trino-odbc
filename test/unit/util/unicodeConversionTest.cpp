#include "gtest/gtest.h"
#include <string>

#include "../../../src/util/stringFromChar.hpp"
#include "../../../src/util/unicodeConversion.hpp"

/*
String literals here are ASCII, with other characters written as
escapes or code units. MSVC reads a source file without a byte order
mark in the ANSI code page, so a UTF-8 "é" in a literal would compile
as two characters.
*/

TEST(UnicodeConversionTest, AsciiRoundTrips) {
  std::u16string wide = utf8ToUtf16("hello");
  EXPECT_EQ(wide, u"hello");
  EXPECT_EQ(utf16ToUtf8(wide.data(), wide.size()), "hello");
}

TEST(UnicodeConversionTest, TwoByteCharacter) {
  // é is U+00E9, which is two bytes in UTF-8. Read one byte at a time,
  // the way the ANSI code page does, it turns into "Ã©".
  std::string utf8    = "caf\xC3\xA9";
  std::u16string wide = utf8ToUtf16(utf8);
  EXPECT_EQ(wide, (std::u16string{u'c', u'a', u'f', 0x00E9}));
  EXPECT_EQ(wide.size(), 4);
  EXPECT_EQ(utf16ToUtf8(wide.data(), wide.size()), utf8);
}

TEST(UnicodeConversionTest, ThreeByteCharacters) {
  // 日本 is U+65E5 U+672C, which are three bytes each in UTF-8.
  std::string utf8    = "\xE6\x97\xA5\xE6\x9C\xAC";
  std::u16string wide = utf8ToUtf16(utf8);
  EXPECT_EQ(wide, (std::u16string{0x65E5, 0x672C}));
  EXPECT_EQ(utf16ToUtf8(wide.data(), wide.size()), utf8);
}

TEST(UnicodeConversionTest, CharacterAboveBmpUsesSurrogatePair) {
  // 😀 is U+1F600, four bytes in UTF-8 and two UTF-16 code units.
  std::string utf8    = "\xF0\x9F\x98\x80";
  std::u16string wide = utf8ToUtf16(utf8);
  ASSERT_EQ(wide.size(), 2);
  EXPECT_EQ(wide[0], 0xD83D);
  EXPECT_EQ(wide[1], 0xDE00);
  EXPECT_EQ(utf16ToUtf8(wide.data(), wide.size()), utf8);
}

TEST(UnicodeConversionTest, InvalidUtf8BecomesReplacementCharacters) {
  // A stray continuation byte, a truncated sequence, and an overlong
  // encoding of '/'. Each bad byte is replaced, and the rest is kept.
  EXPECT_EQ(utf8ToUtf16("a\x80z"), (std::u16string{u'a', 0xFFFD, u'z'}));
  EXPECT_EQ(utf8ToUtf16("a\xC3"), (std::u16string{u'a', 0xFFFD}));
  EXPECT_EQ(utf8ToUtf16("\xC0\xAF"), (std::u16string{0xFFFD, 0xFFFD}));
}

TEST(UnicodeConversionTest, EncodedSurrogateIsInvalidUtf8) {
  // ED A0 80 would decode to U+D800, which UTF-8 doesn't allow.
  EXPECT_EQ(utf8ToUtf16("\xED\xA0\x80"),
            (std::u16string{0xFFFD, 0xFFFD, 0xFFFD}));
}

TEST(UnicodeConversionTest, UnpairedSurrogateBecomesReplacementCharacter) {
  std::u16string wide = u"a";
  wide += static_cast<char16_t>(0xD800);
  wide += u"z";
  EXPECT_EQ(utf16ToUtf8(wide.data(), wide.size()), "a\xEF\xBF\xBDz");
}

TEST(UnicodeConversionTest, StringFromWideCharReadsNullTerminated) {
  std::u16string text = {u'c', u'a', u'f', 0x00E9};
  EXPECT_EQ(stringFromWideChar(text.c_str(), CHAR_IS_NTS), "caf\xC3\xA9");
}

TEST(UnicodeConversionTest, StringFromWideCharReadsGivenLength) {
  EXPECT_EQ(stringFromWideChar(u"abcdef", 3), "abc");
}

TEST(UnicodeConversionTest, StringFromWideCharReadsNullAsEmpty) {
  EXPECT_EQ(stringFromWideChar(nullptr, CHAR_IS_NTS), "");
  EXPECT_EQ(stringFromWideChar(nullptr, 5), "");
}
