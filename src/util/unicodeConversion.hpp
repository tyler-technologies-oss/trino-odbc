#pragma once
#include <string>

/*
Trino sends and receives text as UTF-8. The Unicode (W) ODBC functions
and SQL_C_WCHAR buffers hold UTF-16 instead, which is what Windows
applications use. These convert between the two.
*/

/*
Convert UTF-16 text to UTF-8. The length is a count of UTF-16 code
units, not bytes. A surrogate that isn't part of a pair is written as
U+FFFD, the Unicode replacement character.
*/
std::string utf16ToUtf8(const char16_t* text, size_t length);

/*
Convert UTF-8 text to UTF-16. Bytes that aren't valid UTF-8 are each
written as U+FFFD, the Unicode replacement character.
*/
std::u16string utf8ToUtf16(const std::string& text);

/*
Read the UTF-16 text an application passed to a Unicode (W) ODBC
function, as UTF-8. The length is a count of characters, or
CHAR_IS_NTS if the text is null terminated. A null pointer reads as
an empty string.
*/
std::string stringFromWideChar(const char16_t* text, long textLength);
