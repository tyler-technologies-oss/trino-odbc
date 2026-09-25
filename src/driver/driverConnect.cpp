#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <map>
#include <string>

#include "config/profileReader.hpp"
#include "handles/connHandle.hpp"

#include "../util/delimKvpHelper.hpp"
#include "../util/stringFromChar.hpp"
#include "../util/unicodeConversion.hpp"
#include "../util/valuePtrHelper.hpp"
#include "../util/writeLog.hpp"

/*
The work of SQLDriverConnect and SQLDriverConnectW. The input
connection string has already been read as UTF-8. The output one is
written for the kind of function that was called, and its
BufferLength is a count of characters.
*/
static SQLRETURN driverConnect(SQLHDBC ConnectionHandle,
                               std::string inputConnStr,
                               SQLPOINTER OutConnectionChars,
                               SQLSMALLINT BufferLength,
                               SQLSMALLINT* StringLength2Ptr,
                               TextEncoding encoding) {
  Connection* connection = reinterpret_cast<Connection*>(ConnectionHandle);
  // A new call starts with no diagnostics from the previous one.
  connection->clearError();

  WriteLog(LL_TRACE, "  Input connection string was: " + inputConnStr);

  WriteLog(LL_TRACE, "  Parsing input connection string");
  std::map<std::string, std::string> kvps =
      parseKVPsFromSemicolonDelimStr(inputConnStr);

  if (kvps.count("dsn")) {
    WriteLog(LL_TRACE, "  An explicit DSN was provided to SQLDriverConnect");
    // This means there was an explict DSN passed in. We should use whatever
    // config we can from that DSN's profile
    std::string dsn            = kvps.at("dsn");
    DriverConfig defaultConfig = readDriverConfigFromProfile(dsn);
    std::map<std::string, std::string> defaultKvps =
        driverConfigToKVPs(defaultConfig);

    // Copy the default values into the parsed version of the
    // connection string, but only if the key isn't already present.
    // This will provide the defaults without overwriting any customization.
    for (auto it = defaultKvps.begin(); it != defaultKvps.end(); it++) {
      if (not kvps.count(it->first)) {
        kvps[it->first] = it->second;
      }
    }
    WriteLog(LL_TRACE, "The final connection config was as follows");
    WriteLog(LL_TRACE, kvps);
  } else {
    WriteLog(LL_TRACE,
             "  An explicit DSN was not provided to SQLDriverConnect");
  }

  WriteLog(LL_TRACE, "  Constructing driver config");
  DriverConfig config = driverConfigFromKVPs(kvps);

  // It's kind of unfortunate that we can't set the log level of the driver
  // until we've read the DSN in some way.
  WriteLog(LL_TRACE, "  Setting Log Level");
  setLogLevel(config.getLogLevelEnum());

  WriteLog(LL_TRACE, "  Configuring connection");
  try {
    connection->configure(config);
    connection->connected = true;

    // The output connection string is the input one. It's written
    // whether or not there's room for all of it, along with the length
    // of the whole thing, and a short buffer is reported as truncation.
    WriteLog(LL_TRACE,
             "  Copying input connection string to output connection string");
    bool truncated = writeNullTermCharsToPtr(OutConnectionChars,
                                             inputConnStr,
                                             BufferLength,
                                             StringLength2Ptr,
                                             encoding);
    if (truncated) {
      WriteLog(LL_WARN, "  The output connection string was truncated");
      // 01004 = String data, right truncated.
      connection->setError(ErrorInfo("String data, right truncated", "01004"));
      return SQL_SUCCESS_WITH_INFO;
    }

    WriteLog(LL_TRACE, "  Connection ready");
    return SQL_SUCCESS;
  } catch (std::exception& e) {
    WriteLog(LL_DEBUG,
             "  Error from SQLDriverConnect: " + std::string(e.what()));
    ErrorInfo error = ErrorInfo(e.what(), "HY000");
    connection->setError(error);
    return SQL_ERROR;
  }
}

SQLRETURN SQL_API SQLDriverConnect(SQLHDBC ConnectionHandle,
                                   SQLHWND Windowhandle,
                                   _In_reads_(InConnectionChars)
                                       SQLCHAR* InConnectionChars,
                                   SQLSMALLINT StringLength1,
                                   _Out_writes_opt_(OutConnectionChars)
                                       SQLCHAR* OutConnectionChars,
                                   SQLSMALLINT BufferLength,
                                   _Out_opt_ SQLSMALLINT* StringLength2Ptr,
                                   SQLUSMALLINT DriverCompletion) {
  WriteLog(LL_TRACE, "Entering SQLDriverConnect");
  if (InConnectionChars == nullptr) {
    WriteLog(LL_ERROR, "  ERROR: Connection string input is null");
    return SQL_ERROR;
  }

  WriteLog(LL_TRACE, "  Reading input connection string");
  std::string inputConnStr = stringFromChar(InConnectionChars, StringLength1);
  return driverConnect(ConnectionHandle,
                       inputConnStr,
                       OutConnectionChars,
                       BufferLength,
                       StringLength2Ptr,
                       AnsiText);
}

SQLRETURN SQL_API SQLDriverConnectW(SQLHDBC ConnectionHandle,
                                    SQLHWND Windowhandle,
                                    _In_reads_(StringLength1)
                                        SQLWCHAR* InConnectionChars,
                                    SQLSMALLINT StringLength1,
                                    _Out_writes_opt_(BufferLength)
                                        SQLWCHAR* OutConnectionChars,
                                    SQLSMALLINT BufferLength,
                                    _Out_opt_ SQLSMALLINT* StringLength2Ptr,
                                    SQLUSMALLINT DriverCompletion) {
  WriteLog(LL_TRACE, "Entering SQLDriverConnectW");
  if (InConnectionChars == nullptr) {
    WriteLog(LL_ERROR, "  ERROR: Connection string input is null");
    return SQL_ERROR;
  }

  WriteLog(LL_TRACE, "  Reading input connection string");
  std::string inputConnStr = stringFromWideChar(
      reinterpret_cast<char16_t*>(InConnectionChars), StringLength1);
  return driverConnect(ConnectionHandle,
                       inputConnStr,
                       OutConnectionChars,
                       BufferLength,
                       StringLength2Ptr,
                       UnicodeText);
}
