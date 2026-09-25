#pragma once

#include "windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "unicodeConversion.hpp"

// The Unicode (W) functions pass UTF-16 in SQLWCHAR buffers, which this
// driver reads and writes as char16_t.
static_assert(sizeof(SQLWCHAR) == sizeof(char16_t));

/*
Write a string to the buffer at InfoValuePtr, and the length of
the string to StringLengthPtr.

BufferLength is the size of that buffer in bytes, including the
room needed for the null terminator, exactly as ODBC hands it to
the driver. Nothing is written past it. When the string and its
terminator do not both fit, as much of the string as will fit is
written, the result is still terminated, and this returns true so
the caller can report SQLSTATE 01004 and SQL_SUCCESS_WITH_INFO
the way ODBC requires.

StringLengthPtr always receives the length the whole string would
have had, not the number of bytes written. Applications depend on
that to discover how large a buffer they need and ask again.

If InfoValuePtr is null, this writes only the length. Applications
use that to size a buffer before allocating one, so it is not a
truncation and returns false.
*/
template <class T>
bool writeNullTermStringToPtr(SQLPOINTER InfoValuePtr,
                              const std::string& s,
                              SQLLEN BufferLength,
                              T* StringLengthPtr) {
  const size_t length = s.length();

  if (StringLengthPtr) {
    *StringLengthPtr = static_cast<T>(length);
  }

  if (InfoValuePtr == nullptr) {
    return false;
  }

  if (BufferLength <= 0) {
    // There is no room to write anything, not even a terminator,
    // so leave the caller's buffer alone entirely.
    return length > 0;
  }

  // One byte of the buffer belongs to the null terminator.
  const size_t capacity = static_cast<size_t>(BufferLength) - 1;
  const size_t copied   = std::min(length, capacity);

  char* infoCharPtr = reinterpret_cast<char*>(InfoValuePtr);
  memcpy_s(infoCharPtr, static_cast<size_t>(BufferLength), s.data(), copied);
  infoCharPtr[copied] = '\0';

  return copied < length;
}

/*
The Unicode (W) version of writeNullTermStringToPtr. The string is
UTF-8, as Trino sends it, and is written as UTF-16.

BufferLength and the length written to StringLengthPtr are both in
bytes, as the W functions use for buffers that may hold either a
string or a number, such as SQLGetInfoW's. A character above U+FFFF
takes two UTF-16 code units, and a truncated string never ends
halfway through one.
*/
template <class T>
bool writeNullTermWideStringToPtr(SQLPOINTER InfoValuePtr,
                                  const std::string& s,
                                  SQLLEN BufferLength,
                                  T* StringLengthPtr) {
  std::u16string wide = utf8ToUtf16(s);
  const size_t length = wide.length();

  if (StringLengthPtr) {
    *StringLengthPtr = static_cast<T>(length * sizeof(char16_t));
  }

  if (InfoValuePtr == nullptr) {
    return false;
  }

  // Only whole characters fit, and one of them is the terminator.
  const SQLLEN characterCapacity = BufferLength / sizeof(char16_t);
  if (characterCapacity <= 0) {
    return length > 0;
  }

  size_t copied = std::min(length, static_cast<size_t>(characterCapacity - 1));
  // Don't split a surrogate pair. Half of one is not a character.
  if (copied < length and copied > 0 and wide[copied - 1] >= 0xD800 and
      wide[copied - 1] <= 0xDBFF) {
    copied--;
  }

  char16_t* infoCharPtr = reinterpret_cast<char16_t*>(InfoValuePtr);
  memcpy_s(infoCharPtr,
           static_cast<size_t>(characterCapacity) * sizeof(char16_t),
           wide.data(),
           copied * sizeof(char16_t));
  infoCharPtr[copied] = u'\0';

  return copied < length;
}

/*
Like writeNullTermWideStringToPtr, but BufferLength and the length
written to StringLengthPtr are counts of characters, not bytes. The W
functions use characters for arguments that only ever hold strings,
such as the column name from SQLDescribeColW.
*/
template <class T>
bool writeNullTermWideCharsToPtr(SQLPOINTER InfoValuePtr,
                                 const std::string& s,
                                 SQLLEN BufferLength,
                                 T* StringLengthPtr) {
  SQLLEN lengthInBytes = 0;
  bool truncated       = writeNullTermWideStringToPtr(
      InfoValuePtr, s, BufferLength * sizeof(char16_t), &lengthInBytes);
  if (StringLengthPtr) {
    *StringLengthPtr = static_cast<T>(lengthInBytes / sizeof(char16_t));
  }
  return truncated;
}

/*
Which kind of ODBC function a string is being returned from. The ANSI
functions return UTF-8 text in SQLCHAR buffers. The Unicode functions,
whose names end in W, return UTF-16 text in SQLWCHAR buffers.
*/
enum TextEncoding {
  AnsiText,
  UnicodeText
};

/*
Write a string for either kind of function, where BufferLength is in
bytes. That's the case for buffers that may hold either a string or a
number, like SQLGetInfo's.
*/
template <class T>
bool writeNullTermStringToPtr(SQLPOINTER InfoValuePtr,
                              const std::string& s,
                              SQLLEN BufferLength,
                              T* StringLengthPtr,
                              TextEncoding encoding) {
  if (encoding == UnicodeText) {
    return writeNullTermWideStringToPtr(
        InfoValuePtr, s, BufferLength, StringLengthPtr);
  }
  return writeNullTermStringToPtr(
      InfoValuePtr, s, BufferLength, StringLengthPtr);
}

/*
Write a string for either kind of function, where BufferLength is a
count of characters. That's the case for arguments that only ever
hold strings, like the message from SQLGetDiagRec. For the ANSI
functions a character is a byte.
*/
template <class T>
bool writeNullTermCharsToPtr(SQLPOINTER InfoValuePtr,
                             const std::string& s,
                             SQLLEN BufferLength,
                             T* StringLengthPtr,
                             TextEncoding encoding) {
  if (encoding == UnicodeText) {
    return writeNullTermWideCharsToPtr(
        InfoValuePtr, s, BufferLength, StringLengthPtr);
  }
  return writeNullTermStringToPtr(
      InfoValuePtr, s, BufferLength, StringLengthPtr);
}
