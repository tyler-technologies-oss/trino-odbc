#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class ColumnDescription {
  private:
    std::string name;
    std::string type;
    std::string rawType;
    json typeArguments;

  public:
    ColumnDescription(const json& columnInfo);
    const std::string& getName() const;
    const std::string& getType() const;
    const std::string& getRawType() const;
    const json& getTypeArguments() const;
};

// Build the column JSON that Trino sends with query results, from a
// column name and a type name like "decimal(38,9)". DESCRIBE OUTPUT
// reports column types in that form.
json columnJsonFromTypeName(std::string name, std::string typeName);
