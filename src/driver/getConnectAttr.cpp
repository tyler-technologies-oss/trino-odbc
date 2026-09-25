#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/valuePtrHelper.hpp"
#include "../util/writeLog.hpp"
#include "handles/connHandle.hpp"


/*
The work of SQLGetConnectAttr and SQLGetConnectAttrW. String attributes
are written for the kind of function that was called, and BufferLength
is in bytes for both.
*/
static SQLRETURN getConnectAttr(SQLHDBC ConnectionHandle,
                                SQLINTEGER Attribute,
                                SQLPOINTER Value,
                                SQLINTEGER BufferLength,
                                SQLINTEGER* StringLengthPtr,
                                TextEncoding encoding) {
  WriteLog(LL_TRACE,
           "  Application is requesting connection attribute: " +
               std::to_string(Attribute));
  Connection* connection = reinterpret_cast<Connection*>(ConnectionHandle);
  // A new call starts with no diagnostics from the previous one.
  connection->clearError();
  switch (Attribute) {
    case (SQL_ATTR_CONNECTION_DEAD): { // 1209
      // An integer attribute, so BufferLength is ignored. The driver
      // can't tell whether the server is still reachable without a
      // round trip, so this reports whether it's connected at all.
      if (Value) {
        *reinterpret_cast<SQLUINTEGER*>(Value) =
            connection->connected ? SQL_CD_FALSE : SQL_CD_TRUE;
      }
      break;
    }
    case (SQL_ATTR_CURRENT_CATALOG): {
      // Return an empty string to show that no catalog is assigned.
      // I'm not sure if you can set a specific catalog at the connection level.
      if (writeNullTermStringToPtr(
              Value, "system", BufferLength, StringLengthPtr, encoding)) {
        WriteLog(LL_WARN,
                 "  Exiting SQLGetConnectAttr - the buffer provided for the "
                 "current catalog was too small");
        // 01004 = String data, right truncated. StringLengthPtr holds
        // the length the application needs to allocate to get it all.
        connection->setError(
            ErrorInfo("String data, right truncated", "01004"));
        return SQL_SUCCESS_WITH_INFO;
      }
      break;
    }
    default: {
      WriteLog(LL_ERROR,
               "  ERROR: Application is requesting unimplemented connection "
               "attribute: " +
                   std::to_string(Attribute));
      // HY092 = Invalid attribute/option identifier.
      connection->setError(ErrorInfo("Unknown Connection Attribute", "HY092"));
      return SQL_ERROR;
    }
  }
  return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetConnectAttr(
    SQLHDBC ConnectionHandle,
    SQLINTEGER Attribute,
    _Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
    SQLINTEGER BufferLength,
    _Out_opt_ SQLINTEGER* StringLengthPtr) {
  WriteLog(LL_TRACE, "Entering SQLGetConnectAttr");
  return getConnectAttr(ConnectionHandle,
                        Attribute,
                        Value,
                        BufferLength,
                        StringLengthPtr,
                        AnsiText);
}

SQLRETURN SQL_API SQLGetConnectAttrW(
    SQLHDBC ConnectionHandle,
    SQLINTEGER Attribute,
    _Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
    SQLINTEGER BufferLength,
    _Out_opt_ SQLINTEGER* StringLengthPtr) {
  WriteLog(LL_TRACE, "Entering SQLGetConnectAttrW");
  return getConnectAttr(ConnectionHandle,
                        Attribute,
                        Value,
                        BufferLength,
                        StringLengthPtr,
                        UnicodeText);
}
