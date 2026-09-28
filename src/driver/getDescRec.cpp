#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "../util/writeLog.hpp"

SQLRETURN SQL_API SQLGetDescRec(SQLHDESC DescriptorHandle,
                                SQLSMALLINT RecNumber,
                                _Out_writes_opt_(BufferLength) SQLCHAR* Name,
                                SQLSMALLINT BufferLength,
                                _Out_opt_ SQLSMALLINT* StringLengthPtr,
                                _Out_opt_ SQLSMALLINT* TypePtr,
                                _Out_opt_ SQLSMALLINT* SubTypePtr,
                                _Out_opt_ SQLLEN* LengthPtr,
                                _Out_opt_ SQLSMALLINT* PrecisionPtr,
                                _Out_opt_ SQLSMALLINT* ScalePtr,
                                _Out_opt_ SQLSMALLINT* NullablePtr) {
  WriteLog(LL_TRACE, "Entering SQLGetDescRec");
  WriteLog(LL_ERROR, "  ERROR: SQLGetDescRec unimplemented");
  return SQL_ERROR;
}

SQLRETURN SQL_API SQLGetDescRecW(SQLHDESC DescriptorHandle,
                                 SQLSMALLINT RecNumber,
                                 _Out_writes_opt_(BufferLength) SQLWCHAR* Name,
                                 SQLSMALLINT BufferLength,
                                 _Out_opt_ SQLSMALLINT* StringLengthPtr,
                                 _Out_opt_ SQLSMALLINT* TypePtr,
                                 _Out_opt_ SQLSMALLINT* SubTypePtr,
                                 _Out_opt_ SQLLEN* LengthPtr,
                                 _Out_opt_ SQLSMALLINT* PrecisionPtr,
                                 _Out_opt_ SQLSMALLINT* ScalePtr,
                                 _Out_opt_ SQLSMALLINT* NullablePtr) {
  WriteLog(LL_TRACE, "Entering SQLGetDescRecW");
  WriteLog(LL_ERROR, "  ERROR: SQLGetDescRecW unimplemented");
  return SQL_ERROR;
}
