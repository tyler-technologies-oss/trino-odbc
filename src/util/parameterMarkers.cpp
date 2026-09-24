#include "parameterMarkers.hpp"

int countParameterMarkers(std::string_view sql) {
  int markerCount = 0;
  size_t i        = 0;
  while (i < sql.size()) {
    char current = sql[i];
    char next    = (i + 1 < sql.size()) ? sql[i + 1] : '\0';

    if (current == '\'' or current == '"') {
      // Skip a string literal or a quoted identifier. Both escape
      // their closing quote by doubling it, so a doubled quote is
      // part of the text and not the end of it.
      char quote = current;
      i++;
      while (i < sql.size()) {
        if (sql[i] == quote) {
          if (i + 1 < sql.size() and sql[i + 1] == quote) {
            i += 2;
            continue;
          }
          break;
        }
        i++;
      }
      // Step past the closing quote.
      i++;
    } else if (current == '-' and next == '-') {
      // Skip a line comment, up to the end of the line.
      size_t lineEnd = sql.find('\n', i);
      i = (lineEnd == std::string_view::npos) ? sql.size() : lineEnd + 1;
    } else if (current == '/' and next == '*') {
      // Skip a block comment. Trino does not nest them.
      size_t commentEnd = sql.find("*/", i + 2);
      i = (commentEnd == std::string_view::npos) ? sql.size() : commentEnd + 2;
    } else {
      if (current == '?') {
        markerCount++;
      }
      i++;
    }
  }
  return markerCount;
}
