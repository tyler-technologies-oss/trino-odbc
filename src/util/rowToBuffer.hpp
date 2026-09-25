#include "windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class ColumnToBufferStatus {
  public:
    bool isSuccess        = false;
    bool isVariableLength = false;
    ColumnToBufferStatus(bool isSuccess, bool isVariableLength);
};

/*
Every type can be read as characters, and some applications do that
for numbers (.NET reads BIGINT this way). Trino sends numbers and
booleans as JSON numbers and booleans, not strings, so they have to
be turned into text here.
*/
std::string jsonValueToText(const json& jsonValue);

ColumnToBufferStatus columnToBuffer(SQLSMALLINT cDataType,
                                    SQLSMALLINT odbcDataType,
                                    const json& rowData,
                                    SQLULEN columnNumber,
                                    void* buffer,
                                    SQLLEN bufferLength,
                                    SQLLEN* strLen_or_IndPtr,
                                    SQLCHAR precision,
                                    SQLCHAR scale);
