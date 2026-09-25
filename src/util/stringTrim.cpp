#include "stringTrim.hpp"

#include <algorithm>
#include <cctype>

/*
 Analogous to to python's str.trim(), but in-place.
*/
void trim(std::string& str) {
  // Trim whitespace from the start of the string
  auto firstRealChar = std::ranges::find_if_not(
      str, [](unsigned char c) { return std::isspace(c); });
  str.erase(str.begin(), firstRealChar);

  // Use reverse-iterator to find the last non-space character
  auto lastRealChar =
      std::ranges::find_if_not(str.rbegin(), str.rend(), [](unsigned char c) {
        return std::isspace(c);
      }).base();
  str.erase(lastRealChar, str.end());
}

std::string removeTrailingSemicolons(std::string query) {
  auto lastRealChar =
      std::ranges::find_if_not(
          query.rbegin(),
          query.rend(),
          [](unsigned char c) { return c == ';' or std::isspace(c); })
          .base();
  query.erase(lastRealChar, query.end());
  return query;
}
