#include "parameterToText.hpp"

#include <format>
#include <stdexcept>

#include "../../util/stringReplace.hpp"
#include "../../util/writeLog.hpp"

/*
Return the length and indicator value an application gave for a
parameter. Applications may leave the indicator pointer null, which
means the buffer holds a null terminated string.
*/
static SQLLEN getIndicator(const DescriptorField& field) {
  if (field.bufferStrLenOrIndPtr == nullptr) {
    return SQL_NTS;
  }
  return *field.bufferStrLenOrIndPtr;
}

/*
Convert UTF-16 text, which is what Windows applications pass in a
SQL_C_WCHAR buffer, to the UTF-8 text Trino expects.
*/
static std::string utf16ToUtf8(const uint16_t* text, size_t length) {
  std::string result;
  result.reserve(length);
  for (size_t i = 0; i < length; i++) {
    uint32_t codePoint = text[i];
    // A high surrogate followed by a low surrogate encodes one code
    // point above U+FFFF.
    if (codePoint >= 0xD800 and codePoint <= 0xDBFF and i + 1 < length and
        text[i + 1] >= 0xDC00 and text[i + 1] <= 0xDFFF) {
      codePoint =
          0x10000 + ((codePoint - 0xD800) << 10) + (text[i + 1] - 0xDC00);
      i++;
    }
    if (codePoint < 0x80) {
      result += static_cast<char>(codePoint);
    } else if (codePoint < 0x800) {
      result += static_cast<char>(0xC0 | (codePoint >> 6));
      result += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else if (codePoint < 0x10000) {
      result += static_cast<char>(0xE0 | (codePoint >> 12));
      result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else {
      result += static_cast<char>(0xF0 | (codePoint >> 18));
      result += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
      result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
  }
  return result;
}

/*
Write a SQL_NUMERIC_STRUCT as a decimal number. Its value is a 128 bit
unsigned integer stored least significant byte first, and the scale
says how many of its decimal digits come after the decimal point.
*/
static std::string numericToText(const SQL_NUMERIC_STRUCT* numeric) {
  unsigned char value[SQL_MAX_NUMERIC_LEN];
  for (int i = 0; i < SQL_MAX_NUMERIC_LEN; i++) {
    value[i] = numeric->val[i];
  }

  // Divide the whole value by ten until nothing is left. Each
  // remainder is the next decimal digit, least significant first.
  std::string digits;
  bool valueIsZero = false;
  while (not valueIsZero) {
    int remainder = 0;
    valueIsZero   = true;
    for (int i = SQL_MAX_NUMERIC_LEN - 1; i >= 0; i--) {
      int current = remainder * 256 + value[i];
      value[i]    = static_cast<unsigned char>(current / 10);
      remainder   = current % 10;
      if (value[i] != 0) {
        valueIsZero = false;
      }
    }
    digits.insert(digits.begin(), static_cast<char>('0' + remainder));
  }

  int scale = numeric->scale;
  if (scale > 0) {
    // Pad with leading zeros so there is a digit before the point.
    while (static_cast<int>(digits.size()) <= scale) {
      digits.insert(digits.begin(), '0');
    }
    digits.insert(digits.size() - scale, ".");
  } else if (scale < 0) {
    digits.append(-scale, '0');
  }

  // A sign of 1 means positive and 0 means negative.
  if (numeric->sign == 0) {
    return "-" + digits;
  }
  return digits;
}

std::string descriptorFieldToParameterText(const DescriptorField& field) {
  SQLLEN indicator = getIndicator(field);
  if (indicator == SQL_NULL_DATA) {
    return "NULL";
  }
  // Every other negative indicator asks for something this driver
  // doesn't do, such as sending the value later with SQLPutData
  // (SQL_DATA_AT_EXEC) or using a procedure default (SQL_DEFAULT_PARAM).
  if (indicator < 0 and indicator != SQL_NTS) {
    std::string errorMessage =
        std::format("Unsupported parameter length or indicator: {}", indicator);
    WriteLog(LL_ERROR, errorMessage);
    throw std::runtime_error(errorMessage);
  }

  switch (field.bufferCDataType) {
    // Assume SQL_C_CHAR is a varchar, and surround it with single quotes
    // to match the SQL standard. To avoid SQL injection, we need to escape
    // any single quotes in the string by replacing them with two single quotes.
    case SQL_C_CHAR: { // 1
      const char* text          = static_cast<char*>(field.bufferPtr);
      std::string value         = (indicator == SQL_NTS) ? std::string(text)
                                                         : std::string(text, indicator);
      const std::string escaped = replaceAll(value, "'", "''");
      return std::format("'{}'", escaped);
    }
    // .NET applications, such as Report Builder, bind every string
    // as UTF-16. The indicator holds its length in bytes.
    case SQL_C_WCHAR: { // -8
      const uint16_t* text = static_cast<uint16_t*>(field.bufferPtr);
      size_t length        = 0;
      if (indicator == SQL_NTS) {
        while (text[length] != 0) {
          length++;
        }
      } else {
        length = static_cast<size_t>(indicator) / sizeof(uint16_t);
      }
      const std::string escaped =
          replaceAll(utf16ToUtf8(text, length), "'", "''");
      return std::format("'{}'", escaped);
    }
    // Assume all other types are numeric literals and do not require quotes.
    case SQL_C_FLOAT: { // 7
      return std::format("{}", *static_cast<float*>(field.bufferPtr));
    }
    case SQL_C_DOUBLE: { // 8
      return std::format("{}", *static_cast<double*>(field.bufferPtr));
    }
    case SQL_C_NUMERIC: { // 2
      return numericToText(static_cast<SQL_NUMERIC_STRUCT*>(field.bufferPtr));
    }
    case SQL_C_BIT: { // -7
      return std::format("{}", *static_cast<bool*>(field.bufferPtr));
    }
    case SQL_C_STINYINT:  // -26
    case SQL_C_TINYINT: { // -6
      return std::format("{}", *static_cast<int8_t*>(field.bufferPtr));
    }
    case SQL_C_UTINYINT: { // -28
      return std::format("{}", *static_cast<uint8_t*>(field.bufferPtr));
    }
    case SQL_C_SHORT:    // 5
    case SQL_C_SSHORT: { // -15
      return std::format("{}", *static_cast<int16_t*>(field.bufferPtr));
    }
    case SQL_C_USHORT: { // -17
      return std::format("{}", *static_cast<uint16_t*>(field.bufferPtr));
    }
    case SQL_C_LONG:    // 4
    case SQL_C_SLONG: { // -16
      return std::format("{}", *static_cast<int32_t*>(field.bufferPtr));
    }
    case SQL_C_ULONG: { // -18
      return std::format("{}", *static_cast<uint32_t*>(field.bufferPtr));
    }
    case SQL_C_SBIGINT: { // -25
      return std::format("{}", *static_cast<int64_t*>(field.bufferPtr));
    }
    case SQL_C_UBIGINT: { // -27
      return std::format("{}", *static_cast<uint64_t*>(field.bufferPtr));
    }
    case SQL_C_BINARY: { // -2
      const unsigned char* bytes = static_cast<unsigned char*>(field.bufferPtr);
      SQLLEN length = (indicator == SQL_NTS) ? field.bufferLength : indicator;
      std::string hex;
      for (SQLLEN i = 0; i < length; i++) {
        hex += std::format("{:02X}", bytes[i]);
      }
      return std::format("X'{}'", hex);
    }
    case SQL_C_GUID: { // -11
      SQLGUID* guid = static_cast<SQLGUID*>(field.bufferPtr);
      return std::format("UUID "
                         "'{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}"
                         "{:02x}{:02x}{:02x}'",
                         guid->Data1,
                         guid->Data2,
                         guid->Data3,
                         guid->Data4[0],
                         guid->Data4[1],
                         guid->Data4[2],
                         guid->Data4[3],
                         guid->Data4[4],
                         guid->Data4[5],
                         guid->Data4[6],
                         guid->Data4[7]);
    }
    // ODBC 3 applications use the SQL_C_TYPE_* codes for dates and
    // times. They share their buffer layout with the ODBC 2 codes.
    case SQL_C_TIME:        // 10
    case SQL_C_TYPE_TIME: { // 92
      SQL_TIME_STRUCT* timeStruct =
          static_cast<SQL_TIME_STRUCT*>(field.bufferPtr);
      return std::format("TIME '{:02}:{:02}:{:02}'",
                         timeStruct->hour,
                         timeStruct->minute,
                         timeStruct->second);
    }
    case SQL_C_DATE:        // 9
    case SQL_C_TYPE_DATE: { // 91
      SQL_DATE_STRUCT* dateStruct =
          static_cast<SQL_DATE_STRUCT*>(field.bufferPtr);
      return std::format("DATE '{}-{:02}-{:02}'",
                         dateStruct->year,
                         dateStruct->month,
                         dateStruct->day);
    }
    case SQL_C_TIMESTAMP:        // 11
    case SQL_C_TYPE_TIMESTAMP: { // 93
      SQL_TIMESTAMP_STRUCT* timestampStruct =
          static_cast<SQL_TIMESTAMP_STRUCT*>(field.bufferPtr);
      if (timestampStruct->fraction == 0) {
        return std::format("TIMESTAMP '{}-{:02}-{:02} {:02}:{:02}:{:02}'",
                           timestampStruct->year,
                           timestampStruct->month,
                           timestampStruct->day,
                           timestampStruct->hour,
                           timestampStruct->minute,
                           timestampStruct->second);
      } else {
        // While Trino supports up to 12 digits of sub-second precision, the
        // ODBC SQL_TIMESTAMP_STRUCT only supports 9 digits (nanosecond
        // precision). The 9 is what we have, so we'll pass the full precision
        // available.
        return std::format("TIMESTAMP '{}-{:02}-{:02} {:02}:{:02}:{:02}.{:09}'",
                           timestampStruct->year,
                           timestampStruct->month,
                           timestampStruct->day,
                           timestampStruct->hour,
                           timestampStruct->minute,
                           timestampStruct->second,
                           timestampStruct->fraction);
      }
    }
    default: {
      std::string errorMessage =
          std::format("Unsupported parameter type: {}", field.bufferCDataType);
      WriteLog(LL_ERROR, errorMessage);
      throw std::runtime_error(errorMessage);
    }
  }
}

/*
A parameter is bound once the application has given it a buffer, or
has said its value is NULL, which needs no buffer.
*/
static bool isParameterBound(const DescriptorField& field) {
  if (field.bufferPtr != nullptr) {
    return true;
  }
  return field.bufferStrLenOrIndPtr != nullptr and
         *field.bufferStrLenOrIndPtr == SQL_NULL_DATA;
}

SQLSMALLINT countBoundParameters(Descriptor* paramDescriptor) {
  // Record 0 is the bookmark, so parameters start at 1.
  SQLSMALLINT boundCount = 0;
  for (SQLSMALLINT i = 1; i < paramDescriptor->getFieldCount(); i++) {
    if (not isParameterBound(paramDescriptor->getFieldRef(i))) {
      break;
    }
    boundCount++;
  }
  return boundCount;
}

std::string parameterListText(Descriptor* paramDescriptor,
                              SQLSMALLINT parameterCount) {
  std::string listText;
  for (SQLSMALLINT i = 1; i <= parameterCount; i++) {
    if (i > 1) {
      listText += ", ";
    }
    listText += descriptorFieldToParameterText(paramDescriptor->getFieldRef(i));
  }
  return listText;
}
