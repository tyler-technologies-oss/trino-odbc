#include "gtest/gtest.h"
#include <string>

#include "../../../src/util/valuePtrHelper.hpp"

TEST(ValuePtrHelperTest, CanWriteNullTermStringToPtr) {
  // Setup
  std::string s   = "hello";
  char buffer[20] = {};
  short len       = 0;

  // Test
  bool truncated = writeNullTermStringToPtr(buffer, s, sizeof(buffer), &len);


  // Assert
  EXPECT_STREQ(s.c_str(), buffer);
  EXPECT_EQ(len, s.length());
  EXPECT_FALSE(truncated);
}

TEST(ValuePtrHelperTest, CanWriteEmptyStringToPtr) {
  // Setup
  std::string s  = "";
  char buffer[5] = {};
  short len      = 0;

  // Test
  bool truncated = writeNullTermStringToPtr(buffer, s, sizeof(buffer), &len);

  // Assert
  EXPECT_STREQ(s.c_str(), buffer);
  EXPECT_EQ(len, 0);
  EXPECT_FALSE(truncated);
}

TEST(ValuePtrHelperTest, NullCharInCorrectPosition) {
  /*
  Setup

  The buffer is 9 characters, and the string fits in it with room
  to spare. Then we can look at the resulting buffer to confirm
  that it has the null terminator in the correct place, and that
  the bytes past it were left alone.

  Buffer: "bbbbbbbb"
  String: "aaaa"
  Expected Result: "aaaa\0bbb"
  */
  std::string s  = "aaaa";
  char buffer[9] = {"bbbbbbbb"};
  short len      = 0;

  // Test
  bool truncated = writeNullTermStringToPtr(buffer, s, sizeof(buffer), &len);

  // Assert
  EXPECT_STREQ(s.c_str(), buffer); // Correct data
  EXPECT_EQ(len, s.length());      // Correct length
  EXPECT_FALSE(truncated);
  EXPECT_EQ(buffer[4], '\0'); // Check for null terminator
  EXPECT_EQ(buffer[5], 'b');  // Check for correct padding
  EXPECT_EQ(buffer[6], 'b');  // Check for correct padding
  EXPECT_EQ(buffer[7], 'b');  // Check for correct padding
  EXPECT_EQ(buffer[8], '\0'); // Check for original null terminator.
}

TEST(ValuePtrHelperTest, TruncatesToBufferLength) {
  /*
  Setup

  The string is longer than the buffer it is being written to.
  Only four characters and a terminator fit, and the guard bytes
  past the buffer must be left untouched.

  Reported length stays the length of the whole string, because
  that is how an application discovers the size it needs.
  */
  std::string s = "aaaaaaaa";
  // Five bytes for the value, then guard bytes the write must not
  // reach.
  char buffer[9]           = {"bbbbbbbb"};
  const SQLLEN bufferLimit = 5;
  short len                = 0;

  // Test
  bool truncated = writeNullTermStringToPtr(buffer, s, bufferLimit, &len);

  // Assert
  EXPECT_TRUE(truncated);
  EXPECT_STREQ("aaaa", buffer); // Only what fit
  EXPECT_EQ(len, s.length());   // The length needed, not the length written
  EXPECT_EQ(buffer[4], '\0');   // Terminated inside the buffer
  EXPECT_EQ(buffer[5], 'b');    // Guard byte untouched
  EXPECT_EQ(buffer[6], 'b');    // Guard byte untouched
  EXPECT_EQ(buffer[7], 'b');    // Guard byte untouched
  EXPECT_EQ(buffer[8], '\0');   // Original terminator untouched
}

TEST(ValuePtrHelperTest, ExactFitIsNotTruncated) {
  /*
  Setup

  A buffer that fits the string and its terminator with nothing
  to spare is the boundary case between fitting and truncating.
  */
  std::string s  = "aaaa";
  char buffer[5] = {};
  short len      = 0;

  // Test
  bool truncated = writeNullTermStringToPtr(buffer, s, sizeof(buffer), &len);

  // Assert
  EXPECT_FALSE(truncated);
  EXPECT_STREQ(s.c_str(), buffer);
  EXPECT_EQ(len, s.length());
  EXPECT_EQ(buffer[4], '\0');
}

TEST(ValuePtrHelperTest, OneByteBufferHoldsOnlyTheTerminator) {
  /*
  Setup

  A single byte has room for the terminator and nothing else, so
  the result is an empty string and the write is a truncation.
  */
  std::string s  = "aaaa";
  char buffer[4] = {"bbb"};
  short len      = 0;

  // Test
  bool truncated = writeNullTermStringToPtr(buffer, s, 1, &len);

  // Assert
  EXPECT_TRUE(truncated);
  EXPECT_STREQ("", buffer);
  EXPECT_EQ(len, s.length());
  EXPECT_EQ(buffer[1], 'b'); // Guard byte untouched
  EXPECT_EQ(buffer[2], 'b'); // Guard byte untouched
}

TEST(ValuePtrHelperTest, ZeroLengthBufferIsNotWrittenTo) {
  /*
  Setup

  There is not even room for a terminator, so the buffer has to be
  left exactly as it was found while still reporting the length the
  application would need.
  */
  std::string s  = "aaaa";
  char buffer[4] = {"bbb"};
  short len      = 0;

  // Test
  bool truncated = writeNullTermStringToPtr(buffer, s, 0, &len);

  // Assert
  EXPECT_TRUE(truncated);
  EXPECT_STREQ("bbb", buffer); // Completely untouched
  EXPECT_EQ(len, s.length());
}

TEST(ValuePtrHelperTest, NullPointerReportsLengthOnly) {
  /*
  Setup

  Applications pass a null pointer to ask how large a buffer they
  need before allocating one. That is a length query rather than a
  truncated write, so it is not reported as truncation.
  */
  std::string s = "hello";
  short len     = 0;

  // Test
  bool truncated = writeNullTermStringToPtr(nullptr, s, 0, &len);

  // Assert
  EXPECT_FALSE(truncated);
  EXPECT_EQ(len, s.length());
}

TEST(ValuePtrHelperTest, LengthPointerIsOptional) {
  /*
  Setup

  Callers that already know the length, such as the fixed-size
  SQLSTATE buffer, pass a null length pointer.
  */
  std::string s  = "aaaa";
  char buffer[5] = {};

  // Test
  bool truncated =
      writeNullTermStringToPtr<short>(buffer, s, sizeof(buffer), nullptr);

  // Assert
  EXPECT_FALSE(truncated);
  EXPECT_STREQ(s.c_str(), buffer);
}
