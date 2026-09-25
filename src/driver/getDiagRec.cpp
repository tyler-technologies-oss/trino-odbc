#include "../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include "handles/connHandle.hpp"
#include "handles/descriptorHandle.hpp"
#include "handles/envHandle.hpp"
#include "handles/statementHandle.hpp"

#include "../util/valuePtrHelper.hpp"
#include "../util/writeLog.hpp"

/*
Write a diagnostic the driver recorded on a handle itself, as
opposed to one reported by Trino, as a single diagnostic record.
*/
static SQLRETURN writeHandleError(ErrorInfo& errorInfo,
                                  SQLCHAR* SqlStatePtr,
                                  SQLINTEGER* NativeErrorPtr,
                                  SQLCHAR* MessageTextPtr,
                                  SQLSMALLINT BufferLength,
                                  SQLSMALLINT* TextLengthPtr) {
  // ODBC fixes the SQLSTATE buffer at five characters plus a
  // null terminator, which is the only size it can be given.
  writeNullTermStringToPtr<SQLINTEGER>(
      SqlStatePtr, errorInfo.sqlStateCode, SQL_SQLSTATE_SIZE + 1, nullptr);

  bool truncated = writeNullTermStringToPtr(
      MessageTextPtr, errorInfo.errorMessage, BufferLength, TextLengthPtr);

  if (NativeErrorPtr != nullptr) {
    *NativeErrorPtr =
        -1; // If a valid pointer was provided set the native error code
  }

  // A truncated message is reported by the return code alone.
  // SQLGetDiagRec must not record a diagnostic about itself,
  // which is what makes it safe to call after any failure.
  return truncated ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetDiagRec(SQLSMALLINT HandleType,
                                SQLHANDLE Handle,
                                SQLSMALLINT RecNumber,
                                _Out_writes_opt_(6) SQLCHAR* SqlStatePtr,
                                SQLINTEGER* NativeErrorPtr,
                                _Out_writes_opt_(BufferLength)
                                    SQLCHAR* MessageTextPtr,
                                SQLSMALLINT BufferLength,
                                _Out_opt_ SQLSMALLINT* TextLengthPtr) {
  /*
  Return a series of 1-indexed diagnostic records from various handles.
  If a record is requested beyond what is actually available, return
  SQL_NO_DATA instead.
  */
  WriteLog(LL_ERROR,
           "Entering SQLGetDiagRec. HandleType= " + std::to_string(HandleType));
  switch (HandleType) {
    case (SQL_HANDLE_ENV): {
      Environment* env = reinterpret_cast<Environment*>(Handle);
      WriteLog(LL_ERROR, "  Requesting diagnostics for environment handle");
      WriteLog(LL_ERROR,
               "  Requesting RecNumber: " + std::to_string(RecNumber));
      return SQL_NO_DATA;
    }
    case (SQL_HANDLE_DBC): {
      Connection* conn = reinterpret_cast<Connection*>(Handle);
      WriteLog(LL_ERROR, "  Requesting diagnostics for connection handle");
      WriteLog(LL_ERROR,
               "  Requesting RecNumber: " + std::to_string(RecNumber));
      ErrorInfo errorInfo = conn->getError();
      if (RecNumber == 1 and errorInfo.errorOccurred()) {
        return writeHandleError(errorInfo,
                                SqlStatePtr,
                                NativeErrorPtr,
                                MessageTextPtr,
                                BufferLength,
                                TextLengthPtr);
      } else {
        return SQL_NO_DATA;
      }
    }
    case (SQL_HANDLE_STMT): {
      Statement* statement = reinterpret_cast<Statement*>(Handle);
      WriteLog(LL_ERROR, "  Requesting diagnostics for statement handle");
      WriteLog(LL_ERROR,
               "  Requesting RecNumber: " + std::to_string(RecNumber));

      // A diagnostic the driver recorded on the statement comes from
      // the most recent call, since each call clears it, so it is
      // record 1. Any error from the Trino query follows it.
      ErrorInfo errorInfo        = statement->getError();
      SQLSMALLINT trinoRecNumber = RecNumber;
      if (errorInfo.errorOccurred()) {
        if (RecNumber == 1) {
          return writeHandleError(errorInfo,
                                  SqlStatePtr,
                                  NativeErrorPtr,
                                  MessageTextPtr,
                                  BufferLength,
                                  TextLengthPtr);
        }
        trinoRecNumber = RecNumber - 1;
      }

      if (statement->trinoQuery->hasError()) {
        TrinoOdbcErrorHandler::OdbcError odbcErr =
            statement->trinoQuery->getError();

        // A long Trino error is split across several records. Every
        // record needs its own SQLSTATE, since applications print it
        // for each one, and an unwritten buffer shows up as garbage.
        // ODBC fixes the SQLSTATE buffer at five characters plus a
        // null terminator, which is the only size it can be given.
        writeNullTermStringToPtr<SQLINTEGER>(
            SqlStatePtr, odbcErr.sqlstate, SQL_SQLSTATE_SIZE + 1, nullptr);

        if (NativeErrorPtr) {
          *NativeErrorPtr = odbcErr.native;
        }

        // Build all lines: summary (split by newlines) + stack (each with tab)
        std::vector<std::string> lines;
        {
          std::istringstream summaryStream(
              TrinoOdbcErrorHandler::OdbcErrorToString(odbcErr, false));
          std::string line;
          while (std::getline(summaryStream, line)) {
            lines.push_back(line);
          }
        }
        for (const auto& entry : odbcErr.stack) {
          lines.push_back("\t" + entry);
        }

        // Now, chunk by lines, not by bytes
        size_t chunkSize =
            BufferLength > 0 ? static_cast<size_t>(BufferLength - 1) : 0;
        size_t currentChunk = 1;
        size_t currentLen   = 0;
        std::string chunk;
        bool found = false;

        for (size_t i = 0; i < lines.size();) {
          size_t tempLen = 1;
          std::string tempChunk;
          tempChunk += "\n";
          size_t j = i;
          for (; j < lines.size(); ++j) {
            // +1 for the newline
            size_t lineLen = lines[j].size() + 1;
            if (tempLen + lineLen > chunkSize && tempLen > 0) {
              break; // Don't add this line, chunk is full
            }
            tempChunk += lines[j] + "\n";
            tempLen += lineLen;
          }
          // A line longer than the whole buffer never fits, so give it
          // a record of its own and let the copy below truncate it.
          // Without this the loop never moves past that line, and every
          // record number would be reported as present.
          if (j == i) {
            tempChunk += lines[j] + "\n";
            ++j;
          }
          if (currentChunk == static_cast<size_t>(trinoRecNumber)) {
            chunk = tempChunk;
            found = true;
            break;
          }
          ++currentChunk;
          i = j;
        }

        if (!found || chunk.empty()) {
          return SQL_NO_DATA;
        }

        // Copy the chunk to the output buffer. Only a line too long for
        // the whole buffer is truncated, and then the full length is
        // reported so the application can ask again with more room.
        // As in writeHandleError, the return code alone reports the
        // truncation, since SQLGetDiagRec must not record a diagnostic.
        bool truncated = writeNullTermStringToPtr(
            MessageTextPtr, chunk, BufferLength, TextLengthPtr);

        return truncated ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;
      } else {
        return SQL_NO_DATA;
      }
    }
    case (SQL_HANDLE_DESC): {
      Descriptor* descriptor = reinterpret_cast<Descriptor*>(Handle);
      WriteLog(LL_ERROR, "  Requesting diagnostics for descriptor handle");
      WriteLog(LL_ERROR,
               "  Requesting RecNumber: " + std::to_string(RecNumber));
      return SQL_NO_DATA;
    }
    default: {
      WriteLog(LL_ERROR, "  ERROR: Unknown handle type in SQLGetDiagRec");
      return SQL_ERROR;
    }
  }
}
