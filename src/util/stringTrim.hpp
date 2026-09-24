#pragma once

#include <string>

/*
 Analogous to to python's str.trim(),
 but in-place.
*/
void trim(std::string& str);

/*
 Remove any semicolons, and the whitespace around them, from the end of
 a query. Trino rejects a statement that ends in a semicolon, but tools
 like Report Builder send queries written that way.
*/
std::string removeTrailingSemicolons(std::string query);
