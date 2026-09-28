#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <string>

#include "config/profileReader.hpp"
#include "handles/connHandle.hpp"

#include "../util/stringFromChar.hpp"
#include "../util/unicodeConversion.hpp"
#include "../util/writeLog.hpp"


SQLRETURN SQL_API SQLConnect(SQLHDBC ConnectionHandle,
                             _In_reads_(NameLength1) SQLCHAR* DSNChars,
                             SQLSMALLINT NameLength1,
                             _In_reads_(NameLength2) SQLCHAR* UserNameChars,
                             SQLSMALLINT NameLength2,
                             _In_reads_(NameLength3)
                                 SQLCHAR* AuthenticationChars,
                             SQLSMALLINT NameLength3) {
  WriteLog(LL_TRACE, "Entering SQLConnect");
  std::string dsn        = stringFromChar(DSNChars, NameLength1);
  Connection* connection = reinterpret_cast<Connection*>(ConnectionHandle);
  DriverConfig config    = readDriverConfigFromProfile(dsn);

  // It's kind of unfortunate that we can't set the log level of the driver
  // until we've read the DSN in some way.
  WriteLog(LL_TRACE, "  Setting Log Level");
  setLogLevel(config.getLogLevelEnum());

  WriteLog(LL_TRACE, "  Configuring connection");
  connection->configure(config);

  return SQL_SUCCESS;
}

/*
Exporting SQLConnectW is what makes the Driver Manager treat this as a
Unicode driver. Without it, the Driver Manager converts every string
to and from the Windows ANSI code page, which can't represent most of
Unicode, and asks for query results as SQL_C_CHAR even when the
application asked for SQL_C_WCHAR.
https://learn.microsoft.com/en-us/sql/odbc/reference/develop-app/unicode-drivers
*/
SQLRETURN SQL_API SQLConnectW(SQLHDBC ConnectionHandle,
                              _In_reads_(NameLength1) SQLWCHAR* DSNChars,
                              SQLSMALLINT NameLength1,
                              _In_reads_(NameLength2) SQLWCHAR* UserNameChars,
                              SQLSMALLINT NameLength2,
                              _In_reads_(NameLength3)
                                  SQLWCHAR* AuthenticationChars,
                              SQLSMALLINT NameLength3) {
  WriteLog(LL_TRACE, "Entering SQLConnectW");
  std::string dsn =
      stringFromWideChar(reinterpret_cast<char16_t*>(DSNChars), NameLength1);
  // SQLConnect only reads the DSN.
  return SQLConnect(ConnectionHandle,
                    reinterpret_cast<SQLCHAR*>(dsn.data()),
                    static_cast<SQLSMALLINT>(dsn.size()),
                    nullptr,
                    0,
                    nullptr,
                    0);
}
