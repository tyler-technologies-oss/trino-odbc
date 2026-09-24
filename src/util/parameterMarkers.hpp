#pragma once

#include <string_view>

// Count the `?` parameter markers in a SQL statement. A `?` inside a
// string literal, a quoted identifier, or a comment is not a marker.
int countParameterMarkers(std::string_view sql);
