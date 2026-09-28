#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

// Trino reports an unbounded varchar as having this length.
extern const int64_t UNBOUNDED_VARCHAR_LENGTH;

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
    // The length a varchar was declared with, such as 25 for
    // varchar(25). Zero for an unbounded varchar, or another type.
    int64_t getDeclaredVarcharLength() const;
};

// Build the column JSON that Trino sends with query results, from a
// column name and a type name like "decimal(38,9)". DESCRIBE OUTPUT
// reports column types in that form.
json columnJsonFromTypeName(std::string name, std::string typeName);
