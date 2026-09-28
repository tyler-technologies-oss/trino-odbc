#pragma once

#include "windowsLean.hpp"
#include <string>

// Returns false if no browser could be shown to the user.
bool openURLInDefaultBrowser(const std::string& url);
