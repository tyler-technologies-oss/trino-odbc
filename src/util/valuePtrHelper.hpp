#pragma once

#include "windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <algorithm>
#include <cstring>
#include <string>

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
