#include <windows.h>

#include <gtest/gtest.h>
#include <sql.h>
#include <sqlext.h>
#include <string>

#include "../../src/driver/mappings/parameterToText.hpp"

// ---------------------------------------------------------------------------
// Helper: build a minimal DescriptorField for a given C type / buffer pointer.
// ---------------------------------------------------------------------------
static DescriptorField makeField(SQLSMALLINT cType, void* buf) {
  DescriptorField f;
  f.bufferCDataType = cType;
  f.bufferPtr       = buf;
  return f;
}

// ---------------------------------------------------------------------------
// SQL_C_CHAR – plain strings
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, CharSimpleString) {
  char value[]      = "hello";
  DescriptorField f = makeField(SQL_C_CHAR, value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "'hello'");
}

TEST(DescriptorFieldToParameterTextTest, CharEmptyString) {
  char value[]      = "";
  DescriptorField f = makeField(SQL_C_CHAR, value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "''");
}

// ---------------------------------------------------------------------------
// SQL_C_CHAR – SQL injection: embedded single quotes must be doubled
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, CharEscapesSingleQuote) {
  char value[]      = "O'Brien";
  DescriptorField f = makeField(SQL_C_CHAR, value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "'O''Brien'");
}

TEST(DescriptorFieldToParameterTextTest, CharEscapesMultipleSingleQuotes) {
  char value[]      = "it's a 'test'";
  DescriptorField f = makeField(SQL_C_CHAR, value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "'it''s a ''test'''");
}

TEST(DescriptorFieldToParameterTextTest, CharSqlInjectionAttempt) {
  // Classic injection: closing the string early then appending SQL
  char value[]      = "'; DROP TABLE users; --";
  DescriptorField f = makeField(SQL_C_CHAR, value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "'''; DROP TABLE users; --'");
}

// ---------------------------------------------------------------------------
// SQL_C_LONG – 32-bit signed integer
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, LongPositive) {
  SQLINTEGER value  = 42;
  DescriptorField f = makeField(SQL_C_LONG, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "42");
}

TEST(DescriptorFieldToParameterTextTest, LongNegative) {
  SQLINTEGER value  = -7;
  DescriptorField f = makeField(SQL_C_LONG, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "-7");
}

TEST(DescriptorFieldToParameterTextTest, LongZero) {
  SQLINTEGER value  = 0;
  DescriptorField f = makeField(SQL_C_LONG, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "0");
}

// ---------------------------------------------------------------------------
// SQL_C_FLOAT – single-precision floating point
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, FloatPositive) {
  float value       = 3.14f;
  DescriptorField f = makeField(SQL_C_FLOAT, &value);
  // std::format renders float with enough precision to round-trip; just
  // verify the returned string converts back to the same value.
  const std::string result = descriptorFieldToParameterText(f);
  EXPECT_FLOAT_EQ(std::stof(result), value);
}

TEST(DescriptorFieldToParameterTextTest, FloatNegative) {
  float value              = -1.5f;
  DescriptorField f        = makeField(SQL_C_FLOAT, &value);
  const std::string result = descriptorFieldToParameterText(f);
  EXPECT_FLOAT_EQ(std::stof(result), value);
}

TEST(DescriptorFieldToParameterTextTest, FloatZero) {
  float value       = 0.0f;
  DescriptorField f = makeField(SQL_C_FLOAT, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "0");
}

// ---------------------------------------------------------------------------
// SQL_C_TIME
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, Time) {
  SQL_TIME_STRUCT value = {1, 2, 3}; // 01:02:03
  DescriptorField f     = makeField(SQL_C_TIME, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "TIME '01:02:03'");
}

// ---------------------------------------------------------------------------
// SQL_C_DATE
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, Date) {
  SQL_DATE_STRUCT value = {2001, 8, 22}; // 2001-08-22
  DescriptorField f     = makeField(SQL_C_DATE, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "DATE '2001-08-22'");
}

// ---------------------------------------------------------------------------
// SQL_C_TIMESTAMP – without and with fractional seconds
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, TimestampNoFraction) {
  SQL_TIMESTAMP_STRUCT value = {2020, 6, 10, 15, 55, 23, 0}; // no sub-second
  DescriptorField f          = makeField(SQL_C_TIMESTAMP, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f),
            "TIMESTAMP '2020-06-10 15:55:23'");
}

TEST(DescriptorFieldToParameterTextTest, TimestampWithFraction) {
  SQL_TIMESTAMP_STRUCT value = {
      2020, 6, 10, 15, 55, 23, 500000000}; // .5 seconds
  DescriptorField f = makeField(SQL_C_TIMESTAMP, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f),
            "TIMESTAMP '2020-06-10 15:55:23.500000000'");
}

// ---------------------------------------------------------------------------
// SQL_C_CHAR – a length in the indicator, rather than a null terminator
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, CharUsesIndicatorLength) {
  char value[]           = "hello world";
  SQLLEN length          = 5;
  DescriptorField f      = makeField(SQL_C_CHAR, value);
  f.bufferStrLenOrIndPtr = &length;
  EXPECT_EQ(descriptorFieldToParameterText(f), "'hello'");
}

// ---------------------------------------------------------------------------
// SQL_NULL_DATA – a NULL value, whatever the C type
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, NullIndicator) {
  SQLLEN indicator       = SQL_NULL_DATA;
  DescriptorField f      = makeField(SQL_C_TYPE_TIMESTAMP, nullptr);
  f.bufferStrLenOrIndPtr = &indicator;
  EXPECT_EQ(descriptorFieldToParameterText(f), "NULL");
}

TEST(DescriptorFieldToParameterTextTest, DataAtExecutionIsRejected) {
  SQLINTEGER value       = 1;
  SQLLEN indicator       = SQL_DATA_AT_EXEC;
  DescriptorField f      = makeField(SQL_C_LONG, &value);
  f.bufferStrLenOrIndPtr = &indicator;
  EXPECT_THROW(descriptorFieldToParameterText(f), std::runtime_error);
}

// ---------------------------------------------------------------------------
// SQL_C_WCHAR – UTF-16 strings, which .NET binds for every string
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, WideCharNullTerminated) {
  SQLWCHAR value[]  = {'O', '\'', 'B', 0};
  DescriptorField f = makeField(SQL_C_WCHAR, value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "'O''B'");
}

TEST(DescriptorFieldToParameterTextTest, WideCharUsesByteLength) {
  SQLWCHAR value[]       = {'a', 'b', 'c', 'd', 0};
  SQLLEN length          = 2 * sizeof(SQLWCHAR);
  DescriptorField f      = makeField(SQL_C_WCHAR, value);
  f.bufferStrLenOrIndPtr = &length;
  EXPECT_EQ(descriptorFieldToParameterText(f), "'ab'");
}

TEST(DescriptorFieldToParameterTextTest, WideCharConvertsToUtf8) {
  // "é" is U+00E9, and "😀" is U+1F600, which needs a surrogate pair.
  SQLWCHAR value[]  = {0x00E9, 0xD83D, 0xDE00, 0};
  DescriptorField f = makeField(SQL_C_WCHAR, value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "'\xC3\xA9\xF0\x9F\x98\x80'");
}

// ---------------------------------------------------------------------------
// SQL_C_TYPE_* – the ODBC 3 date and time codes
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, TypeTimestamp) {
  SQL_TIMESTAMP_STRUCT value = {2026, 9, 24, 5, 16, 49, 0};
  DescriptorField f          = makeField(SQL_C_TYPE_TIMESTAMP, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f),
            "TIMESTAMP '2026-09-24 05:16:49'");
}

TEST(DescriptorFieldToParameterTextTest, TypeDate) {
  SQL_DATE_STRUCT value = {2001, 8, 22};
  DescriptorField f     = makeField(SQL_C_TYPE_DATE, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "DATE '2001-08-22'");
}

TEST(DescriptorFieldToParameterTextTest, TypeTime) {
  SQL_TIME_STRUCT value = {1, 2, 3};
  DescriptorField f     = makeField(SQL_C_TYPE_TIME, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "TIME '01:02:03'");
}

// ---------------------------------------------------------------------------
// SQL_C_NUMERIC – a scaled 128 bit integer, which .NET binds for decimal
// ---------------------------------------------------------------------------
static SQL_NUMERIC_STRUCT
makeNumeric(uint64_t value, SQLSCHAR scale, SQLCHAR sign) {
  SQL_NUMERIC_STRUCT numeric = {};
  numeric.precision          = 38;
  numeric.scale              = scale;
  numeric.sign               = sign;
  for (int i = 0; i < 8; i++) {
    numeric.val[i] = static_cast<SQLCHAR>((value >> (8 * i)) & 0xFF);
  }
  return numeric;
}

TEST(DescriptorFieldToParameterTextTest, NumericWithScale) {
  SQL_NUMERIC_STRUCT value = makeNumeric(12345, 2, 1);
  DescriptorField f        = makeField(SQL_C_NUMERIC, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "123.45");
}

TEST(DescriptorFieldToParameterTextTest, NumericNegativeBelowOne) {
  SQL_NUMERIC_STRUCT value = makeNumeric(5, 3, 0);
  DescriptorField f        = makeField(SQL_C_NUMERIC, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "-0.005");
}

TEST(DescriptorFieldToParameterTextTest, NumericZero) {
  SQL_NUMERIC_STRUCT value = makeNumeric(0, 0, 1);
  DescriptorField f        = makeField(SQL_C_NUMERIC, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "0");
}

TEST(DescriptorFieldToParameterTextTest, NumericLargerThan64Bits) {
  // 2^64, which only fits once the ninth byte is used.
  SQL_NUMERIC_STRUCT value = makeNumeric(0, 0, 1);
  value.val[8]             = 1;
  DescriptorField f        = makeField(SQL_C_NUMERIC, &value);
  EXPECT_EQ(descriptorFieldToParameterText(f), "18446744073709551616");
}

// ---------------------------------------------------------------------------
// countBoundParameters / parameterListText
// ---------------------------------------------------------------------------
TEST(DescriptorFieldToParameterTextTest, ParameterListStopsAtCount) {
  SQLINTEGER first  = 1;
  SQLINTEGER second = 2;
  Descriptor descriptor;
  DescriptorField firstField  = makeField(SQL_C_LONG, &first);
  DescriptorField secondField = makeField(SQL_C_LONG, &second);
  descriptor.setField(1, firstField);
  descriptor.setField(2, secondField);
  EXPECT_EQ(countBoundParameters(&descriptor), 2);
  EXPECT_EQ(parameterListText(&descriptor, 1), "1");
  EXPECT_EQ(parameterListText(&descriptor, 2), "1, 2");
}

TEST(DescriptorFieldToParameterTextTest, CountStopsAtFirstUnboundParameter) {
  SQLINTEGER value = 1;
  Descriptor descriptor;
  DescriptorField field = makeField(SQL_C_LONG, &value);
  descriptor.setField(1, field);
  descriptor.setField(3, field);
  EXPECT_EQ(countBoundParameters(&descriptor), 1);
}
