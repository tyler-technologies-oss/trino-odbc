#include "columnDescription.hpp"

#include <algorithm>
#include <cctype>

#include "../util/stringSplitAndTrim.hpp"

// Trino reports an unbounded varchar as having this length.
const int64_t UNBOUNDED_VARCHAR_LENGTH = 2147483647;

ColumnDescription::ColumnDescription(const json& columnInfo) {
  this->name          = columnInfo["name"];
  this->type          = columnInfo["type"];
  this->rawType       = columnInfo["typeSignature"]["rawType"];
  this->typeArguments = columnInfo["typeSignature"]["arguments"];
}

const std::string& ColumnDescription::getName() const {
  return this->name;
}

const std::string& ColumnDescription::getType() const {
  return this->type;
}

const std::string& ColumnDescription::getRawType() const {
  return this->rawType;
}

const json& ColumnDescription::getTypeArguments() const {
  return this->typeArguments;
}

json columnJsonFromTypeName(std::string name, std::string typeName) {
  // The raw type is the type name without its parenthesized arguments,
  // so "timestamp(3) with time zone" becomes "timestamp with time zone".
  std::string rawType = typeName;
  std::string argumentText;
  size_t openIndex = typeName.find('(');
  if (openIndex != std::string::npos) {
    size_t closeIndex = typeName.size();
    int depth         = 0;
    for (size_t i = openIndex; i < typeName.size(); i++) {
      if (typeName[i] == '(') {
        depth++;
      } else if (typeName[i] == ')') {
        depth--;
        if (depth == 0) {
          closeIndex = i;
          break;
        }
      }
    }
    argumentText = typeName.substr(openIndex + 1, closeIndex - openIndex - 1);
    rawType      = typeName.substr(0, openIndex);
    if (closeIndex < typeName.size()) {
      rawType += typeName.substr(closeIndex + 1);
    }
  }

  // The driver only reads numeric arguments, such as a varchar length or
  // a decimal's precision and scale. Other arguments, like the element
  // type of an array, are left out.
  json arguments = json::array();
  for (std::string argument : stringSplitAndTrim(argumentText, ',')) {
    bool isNumber = not argument.empty() and
                    std::all_of(argument.begin(), argument.end(), [](char c) {
                      return std::isdigit(static_cast<unsigned char>(c));
                    });
    if (isNumber) {
      arguments.push_back({{"kind", "LONG"}, {"value", std::stoll(argument)}});
    }
  }
  if (rawType == "varchar" and arguments.empty()) {
    arguments.push_back(
        {{"kind", "LONG"}, {"value", UNBOUNDED_VARCHAR_LENGTH}});
  }

  return {{"name", name},
          {"type", typeName},
          {"typeSignature", {{"rawType", rawType}, {"arguments", arguments}}}};
}
