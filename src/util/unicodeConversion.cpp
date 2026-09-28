#include "unicodeConversion.hpp"

#include <cstdint>

#include "stringFromChar.hpp"

static constexpr char32_t REPLACEMENT_CHARACTER = 0xFFFD;

static void appendUtf8(std::string& result, char32_t codePoint) {
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

std::string utf16ToUtf8(const char16_t* text, size_t length) {
  std::string result;
  result.reserve(length);
  for (size_t i = 0; i < length; i++) {
    char32_t codePoint   = text[i];
    bool isHighSurrogate = codePoint >= 0xD800 and codePoint <= 0xDBFF;
    bool isLowSurrogate  = codePoint >= 0xDC00 and codePoint <= 0xDFFF;
    // A high surrogate followed by a low surrogate encodes one code
    // point above U+FFFF.
    if (isHighSurrogate and i + 1 < length and text[i + 1] >= 0xDC00 and
        text[i + 1] <= 0xDFFF) {
      codePoint =
          0x10000 + ((codePoint - 0xD800) << 10) + (text[i + 1] - 0xDC00);
      i++;
    } else if (isHighSurrogate or isLowSurrogate) {
      codePoint = REPLACEMENT_CHARACTER;
    }
    appendUtf8(result, codePoint);
  }
  return result;
}

std::u16string utf8ToUtf16(const std::string& text) {
  std::u16string result;
  result.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    unsigned char lead = static_cast<unsigned char>(text[i]);

    // The lead byte says how many continuation bytes follow, and
    // holds the high bits of the code point.
    size_t continuationCount = 0;
    char32_t codePoint       = 0;
    char32_t smallestAllowed = 0;
    if (lead < 0x80) {
      codePoint = lead;
    } else if (lead >= 0xC2 and lead <= 0xDF) {
      continuationCount = 1;
      codePoint         = lead & 0x1F;
      smallestAllowed   = 0x80;
    } else if (lead >= 0xE0 and lead <= 0xEF) {
      continuationCount = 2;
      codePoint         = lead & 0x0F;
      smallestAllowed   = 0x800;
    } else if (lead >= 0xF0 and lead <= 0xF4) {
      continuationCount = 3;
      codePoint         = lead & 0x07;
      smallestAllowed   = 0x10000;
    } else {
      // A stray continuation byte, or a lead byte UTF-8 never uses.
      result += static_cast<char16_t>(REPLACEMENT_CHARACTER);
      i++;
      continue;
    }

    bool isValid = i + continuationCount < text.size();
    for (size_t j = 1; isValid and j <= continuationCount; j++) {
      unsigned char next = static_cast<unsigned char>(text[i + j]);
      if ((next & 0xC0) != 0x80) {
        isValid = false;
      } else {
        codePoint = (codePoint << 6) | (next & 0x3F);
      }
    }
    // Reject overlong encodings, UTF-16 surrogates, and anything
    // beyond the last Unicode code point.
    if (isValid and (codePoint < smallestAllowed or
                     (codePoint >= 0xD800 and codePoint <= 0xDFFF) or
                     codePoint > 0x10FFFF)) {
      isValid = false;
    }
    if (not isValid) {
      result += static_cast<char16_t>(REPLACEMENT_CHARACTER);
      i++;
      continue;
    }

    if (codePoint < 0x10000) {
      result += static_cast<char16_t>(codePoint);
    } else {
      // Code points above U+FFFF need a surrogate pair.
      codePoint -= 0x10000;
      result += static_cast<char16_t>(0xD800 + (codePoint >> 10));
      result += static_cast<char16_t>(0xDC00 + (codePoint & 0x3FF));
    }
    i += continuationCount + 1;
  }
  return result;
}

std::string stringFromWideChar(const char16_t* text, long textLength) {
  if (text == nullptr) {
    return "";
  }
  size_t length = 0;
  if (textLength == CHAR_IS_NTS) {
    while (text[length] != 0) {
      length++;
    }
  } else if (textLength > 0) {
    length = static_cast<size_t>(textLength);
  }
  return utf16ToUtf8(text, length);
}
