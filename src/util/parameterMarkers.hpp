#pragma once

#include <string_view>

// Count the `?` parameter markers in a SQL statement. A `?` inside a
// string literal, a quoted identifier, or a comment is not a marker.
int countParameterMarkers(std::string_view sql);

// Is this a Trino PREPARE statement? Its markers belong to the
// statement being prepared, and get their values from a later
// EXECUTE, so they are never filled from bound parameters.
bool isPrepareStatement(std::string_view sql);

// Count the markers that bound parameters fill when the statement
// runs. This is every marker, except in a PREPARE statement.
int countParametersToBind(std::string_view sql);
