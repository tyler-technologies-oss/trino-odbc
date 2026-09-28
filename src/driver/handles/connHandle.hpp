#pragma once

#include "../../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>
#include <vector>

#include "../../trinoAPIWrapper/connectionConfig.hpp"
#include "../config/driverConfig.hpp"
#include "handleErrorInfo.hpp"
#include "statementHandle.hpp"

class Connection {
  private:
    EnvironmentConfig* environmentConfig = nullptr;
    ErrorInfo errorInfo;

  public:
    Connection(EnvironmentConfig* environmentConfig);
    ~Connection();
    void configure(DriverConfig config);
    bool connected                     = false;
    ConnectionConfig* connectionConfig = nullptr;
    void disconnect();

    SQLINTEGER ATTR_AutoCommitMode = SQL_AUTOCOMMIT_ON;
    SQLUINTEGER ATTR_LoginTimeout  = 0;

    // Describe varchars with no length as SQL_LONGVARCHAR. Set from the
    // DSN's unboundedVarchar setting, and given to each statement.
    bool unboundedVarcharAsLong = false;

    std::string getServerVersion();
    void setError(ErrorInfo errorInfo);
    ErrorInfo getError();
    void clearError();

    void checkInputs(DriverConfig config);
};
