#pragma once
#include "../../util/windowsLean.hpp"
#include <sql.h>
#include <sqlext.h>

#include <functional>

#include "descriptorHandle.hpp"
#include "handleErrorInfo.hpp"

#include "../../trinoAPIWrapper/connectionConfig.hpp"
#include "../../trinoAPIWrapper/trinoQuery.hpp"

class Statement {
  private:
    void columnsChangedCallback(TrinoQuery* trinoQuery);
    // SQLFetch needs us to keep track of our position in iterating
    // the results so subsequent calls to fetch can return data
    // starting at the right place. We start at position 0, which
    // indicates that no rows are ready to process.
    SQLLEN fetchedPosition = -1;
    ErrorInfo errorInfo;

    // Trino Query ID
    std::string queryId;

  public:
    Statement(ConnectionConfig* connectionConfig);
    ~Statement();
    // Has this statement been executed?
    bool executed = false;
    // Did we confirm execution of this statement to SQLFetch?
    bool fetchExecuteConfirmed = false;
    // The underlying trino query utility class.
    TrinoQuery* trinoQuery;
    // The SQL text given to the latest SQLPrepare or SQLExecDirect,
    // before the driver wrapped it in anything. SQLExecute and
    // SQLNumParams count its parameter markers.
    std::string statementText;
    // Was statementText given to SQLPrepare, so SQLExecute can run it?
    // SQLExecDirect replaces any prepared statement, so it clears this.
    bool prepared = false;
    // The result columns Trino described for the prepared statement,
    // kept so closing the cursor doesn't lose them.
    json preparedColumns;
    // The method used in SQLFetch for polling trino.
    TrinoQueryPollMode fetchPollMode = UntilNewData;

    // SQLGetData returns a text value too long for the application's
    // buffer in parts, over several calls. These track which column of
    // the current row is being read that way, as which C type, how many
    // characters of it have been returned, and whether all of it has.
    // SQLFetch resets them for each new row.
    SQLUSMALLINT getDataColumn = 0;
    SQLSMALLINT getDataCType   = 0;
    size_t getDataOffset       = 0;
    bool getDataFinished       = false;
    void resetGetDataPosition();

    // The ODBC protocol assumes these descriptors are
    // instantiated on all statements.
    Descriptor* appRowDesc;
    Descriptor* impRowDesc;
    Descriptor* appParamDesc;
    Descriptor* impParamDesc;

    void closeCursor();
    void terminate();
    Descriptor* getRowDescriptor();
    Descriptor* getParamDescriptor();
    SQLLEN getFetchedPosition();
    void setFetchedPosition(SQLLEN pos);

    void setError(ErrorInfo errorInfo);
    ErrorInfo getError();
    void clearError();

    void setQueryId(const std::string& id);
};
