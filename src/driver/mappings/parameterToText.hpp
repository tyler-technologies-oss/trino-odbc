#pragma once

#include "../../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <string>

#include "../handles/descriptorHandle.hpp"

// Render the value bound to a parameter as a Trino SQL literal.
std::string descriptorFieldToParameterText(const DescriptorField& field);

// Count the parameters bound in a row, starting from parameter 1
// and stopping at the first one that isn't bound.
SQLSMALLINT countBoundParameters(Descriptor* paramDescriptor);

// Render the first `parameterCount` bound parameters as a comma
// separated list of literals, ready to follow a USING keyword.
std::string parameterListText(Descriptor* paramDescriptor,
                              SQLSMALLINT parameterCount);
