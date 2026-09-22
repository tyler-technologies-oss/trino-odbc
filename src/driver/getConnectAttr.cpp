#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/valuePtrHelper.hpp"
#include "../util/writeLog.hpp"
#include "handles/connHandle.hpp"


SQLRETURN SQL_API SQLGetConnectAttr(
    SQLHDBC ConnectionHandle,
    SQLINTEGER Attribute,
    _Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
    SQLINTEGER BufferLength,
    _Out_opt_ SQLINTEGER* StringLengthPtr) {
  WriteLog(LL_TRACE, "Entering SQLGetConnectAttr");
  WriteLog(LL_TRACE,
           "  Application is requesting connection attribute: " +
               std::to_string(Attribute));
  switch (Attribute) {
    case (SQL_ATTR_CONNECTION_DEAD): {
    }
    case (SQL_ATTR_CURRENT_CATALOG): {
      // Return an empty string to show that no catalog is assigned.
      // I'm not sure if you can set a specific catalog at the connection level.
      if (writeNullTermStringToPtr(
              Value, "system", BufferLength, StringLengthPtr)) {
        WriteLog(LL_WARN,
                 "  Exiting SQLGetConnectAttr - the buffer provided for the "
                 "current catalog was too small");
        // 01004 = String data, right truncated. StringLengthPtr holds
        // the length the application needs to allocate to get it all.
        Connection* connection =
            reinterpret_cast<Connection*>(ConnectionHandle);
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
      return SQL_ERROR;
    }
  }
  return SQL_SUCCESS;
}
