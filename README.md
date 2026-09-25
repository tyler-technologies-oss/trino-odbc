# Project Overview: Trino ODBC Driver (Partial Implementation)

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)]()
[![Latest Release](https://img.shields.io/github/v/release/trinodb/trino-odbc.svg)]()
[![Build Status](https://img.shields.io/github/actions/workflow/status/trinodb/trino-odbc/make_release.yml.svg)]()
[![Contributors](https://img.shields.io/github/contributors/trinodb/trino-odbc.svg)]()
[![Code Style](https://img.shields.io/badge/Code%20Style-Clang%20Format-blue.svg)]()

## Description

This open-source project is a partially complete ODBC driver for the
[Trino distributed SQL engine](https://trino.io/). It implements the
essential portions of the ODBC core specification required to enable
Microsoft Excel and Microsoft PowerBI Desktop to connect to and
execute queries against a Trino server. Four authentication methods
are supported: no authentication, External Authentication, the OIDC
Client Credential flow, and the OIDC Device Authorization (Device
Flow) grant.
The driver was developed to address a narrow gap in connectivity
options for these tools on the Windows operating system within
the Open Source Trino community.

Note: this project is not sponsored by or affiliated with
Microsoft in any way.

### :warning: Disclaimer: Scope and Limitations :warning:

This driver is tested to work within a very narrow scope on Windows
PCs and servers that utilize BI tools. Specifically, we target
compatibility with Microsoft Excel and PowerBI Desktop with External
Authentication, Client Credential Auth, and Device Flow. Other tools,
forms of authentication, and functionality of the ODBC specification
were left unimplemented if not required by these tools. Functionality on
external systems or configurations may vary, and compatibility is
not guaranteed.  For users needing cross-platform compatibility or
full ODBC specification support, you should consider a feature-complete
commercially available ODBC driver for Trino.

### Alternatives and Performance Considerations

For PowerBI users, there is also the
[Trino PowerBI Connector](https://github.com/CreativeDataEU/PowerBITrinoConnector)
project, which works well for many use cases. In our experience,
it was found to exhibit slow loading performance on datasets containing
hundreds of millions of rows of data. This appears to be
a limitation in the performance of the Microsoft Power Query
language, and is not a reflection on the quality of work
in the PowerBITrinoConnector project, which is fantastic!

### Contributing

Please see [our Contributing guide](./CONTRIBUTING.md) for more information.

### Known Limitations

- Only compiled for Microsoft Windows
- Only supports reading data, not writing/transacting data.
- Does not support most forms of Trino authentication including password authentication
- Non-ASCII text is only reliable through the wide-character (`W`) ODBC
  functions and `SQL_C_WCHAR` buffers, which is what .NET, Power BI and most
  Windows applications use. Through the ANSI functions and `SQL_C_CHAR`
  buffers, text is Trino's UTF-8 as it is, not converted to the Windows code
  page, so an ANSI application sees non-ASCII characters as mojibake unless
  it reads the bytes as UTF-8. See "Unicode" below.
- Text columns are reported as `SQL_VARCHAR`/`SQL_CHAR`, not
  `SQL_WVARCHAR`/`SQL_WCHAR`, even though they can be read as `SQL_C_WCHAR`.
- Supports prepared statements (SQLPrepare, SQLExecute, SQLBindParameter) only
  in a limited form. See the "Prepared Statements and Parameters" section below
  for what is and is not supported.
- Does not support ODBC conformance Level 1 or Level 2
  - [About Conformance Levels](https://learn.microsoft.com/en-us/sql/odbc/reference/develop-app/interface-conformance-levels)
  - It does not __completely__ support the Core conformance level, but is close.
- Does not support SQL Transactions. SQLEndTran is implemented as a no-op that
  reports success; the Trino transaction headers are not handled.
- Does not support binding and fetching multiple rows in a single call (SQLFetch with array size greater than 1)
- Does not support non-sequential data fetching (SQLFetchScroll/SQLExtendedFetch)
- Does not support iteratively discovering and enumerating connection attributes (SQLBrowseConnect)
- All columns are reported as being nullable, regardless of whether they are
  actually nullable or not.

There are many more limitations not mentioned here. The ODBC specification
is quite large and it's very difficult to tell what ODBC features a
given application will require by reading the spec. None of the above
features are required for Microsoft PowerBI Desktop or Microsoft Excel to
use the driver to read data from Trino using Client Credential Auth,
External Authentication, or Device Flow.


### Prepared Statements and Parameters

The driver implements prepared statements by mapping them onto Trino's
own `PREPARE` and `EXECUTE` SQL statements. `SQLPrepare` submits a
`PREPARE <generated name> FROM <your query>` statement to Trino and polls
until Trino reports the statement as prepared. It then runs
`DESCRIBE OUTPUT <generated name>`, so `SQLNumResultCols`,
`SQLDescribeCol`, and `SQLColAttribute` report the result columns before
`SQLExecute` is called, as Power BI expects. If Trino rejects the query
at either step, `SQLPrepare` returns `SQL_ERROR` and the reason is
available from `SQLGetDiagRec`. `SQLBindParameter` records
the bound application buffer in the statement's parameter descriptor.
`SQLExecute` then builds an `EXECUTE "<generated name>" USING ...`
statement, rendering each bound parameter as a SQL literal.

Applications can also bind parameters and call `SQLExecDirect` without
`SQLPrepare`, which is what .NET applications such as Report Builder do.
The driver then runs `EXECUTE IMMEDIATE '<your query>' USING ...`, which
needs Trino 418 or later. A query that has `?` markers but no bound
parameters is sent to Trino unchanged.

What this means in practice:

- Trailing semicolons are removed from the query before it is sent,
  since Trino rejects them. This applies to both `SQLPrepare` and
  `SQLExecDirect`, so queries such as `SELECT 1;` work.
- Parameters are interpolated into SQL text as literals by the driver,
  not sent to Trino as separate parameter values. Strings are single-quoted
  with embedded single quotes doubled.
- The driver passes one bound parameter for each `?` marker in the query,
  ignoring markers inside string literals, quoted identifiers, and
  comments. If fewer parameters are bound than there are markers, the
  query fails with SQLSTATE `07002`.
- Only input parameters are handled. The `InputOutputType` argument to
  `SQLBindParameter` is logged but otherwise ignored, so output and
  input/output parameters do not work.
- Supported parameter C types are `SQL_C_CHAR`, `SQL_C_WCHAR`,
  `SQL_C_FLOAT`, `SQL_C_DOUBLE`, `SQL_C_NUMERIC`, `SQL_C_BIT`, the
  signed/unsigned tinyint, short, long, and bigint types, `SQL_C_BINARY`,
  `SQL_C_GUID`, and the date, time, and timestamp types (both the
  `SQL_C_TYPE_*` and the older `SQL_C_DATE`/`SQL_C_TIME`/`SQL_C_TIMESTAMP`
  codes). Any other C type makes the query fail with `SQL_ERROR`.
- A `StrLen_or_IndPtr` of `SQL_NULL_DATA` passes `NULL`.
- `SQLParamData` and `SQLPutData` are present but return `SQL_ERROR`, so
  data-at-execution parameters do not work.
- `SQLDescribeParam` is not implemented.


### Identifying Specific Limitations

The best way to find what if anything is missing is to give it a try!
This is relatively easy to do.

1. Install this driver.
1. Configure a DSN for this driver and enable trace-level logging in the DSN config.
1. Enable tracing in the driver manager as well.
1. Trigger an error when your application tries to use this driver.
1. Review any error dialog boxes from the application, if they appear
1. Review the driver logs (C:\temp\odbclog.txt) for the word "error"
1. Review the ODBC Driver manager trace logs for the word "error"


From here, you'll often find precisely what features were required
but were not yet implemented. Once you know what's missing, a review
of the ODBC specification's documentation typically explains
what the missing functionality accomplishes. From there, it's
just a matter of writing the code to enable it.


## Developer Prerequisites

We recommend using Visual Studio Community (Not VSCode) for developing
this project. Even in a commercial organization, Visual Studio Community
is free to use for contributing to open source projects such as this one:
https://visualstudio.microsoft.com/vs/community/.

This ODBC driver makes use of the vcpkg tool for CMake. Follow the instructions
[here](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-cmd)
to set it up. `vcpkg.json` declares the dependencies that vcpkg will acquire:
libcurl (with the openssl feature), nlohmann-json, and googletest.

The driver is built as C++20 and uses `std::format`, so a toolchain with
C++20 library support is required. `CMakePresets.json` defines four presets,
which are the configurations this project is built and tested against:

* `x64-debug`
* `x64-release`
* `x86-debug`
* `x86-release`

Debug configurations compile with the `DEBUG` preprocessor definition, which
the test suite uses to pick which DSN to connect to. See "Testing and
Debugging" below.

The installer package makes use of WiX. Installation and configuration of WiX is
described in the "Installing" section below.

## About the ODBC paradigm

Most applications that use ODBC do so through a driver manager rather than interacting
directly with the driver. The driver manager is provided by the operating system or
an operating system package (like UnixODBC). The driver manager intercepts calls
from an application to execute queries, read data, describe columns, or otherwise.
Then the driver manager, with knowledge of the actual capabilities of any given
driver, translates those calls into the appropriate function calls for the
driver. It functions as a kind of normalizing intermediary. This allows
application writers not to need to understand the differences between the ODBC 2,
3.0, 3.5, and 3.8 specs to use an ODBC driver. They simply call the functions
they want to use, and either the driver implements enough to do it or it does not.

In order to enable this functionality in the driver, the driver's author must
supply information about the driver's capabilities to the driver manager. This
is done by implementing a pair of functions. The first is `SQLGetInfo`, which
provides a variety of information about the driver and its capabilities. One
of the things the driver manager can request from `SQLGetInfo` is the
`SQL_DRIVER_ODBC_VER`, which allows the driver manager to infer what level of
capabilities should be present in the driver. The second critical function
is `SQLGetFunctions`, which provides information to the driver manager
about the specific set of functions that are available from the driver.
Not every driver implements every single function in the ODBC spec, and
this driver is no exception. `SQLGetFunctions` provides a way for the
driver manager to ask if a given function is supported or not.

### Unicode

The Windows Driver Manager treats a driver as a Unicode driver only if it
exports `SQLConnectW`. This driver exports the wide-character (`W`) version
of each ODBC function next to its ANSI version, and both share one
implementation. Applications that call the `W` functions get UTF-16 text:
SQL text, catalog names, column names, diagnostics and `SQL_C_WCHAR` data
are converted to and from Trino's UTF-8 by the driver. If a driver exported
only ANSI functions, the Driver Manager would convert `SQL_C_WCHAR` requests
to `SQL_C_CHAR` and widen the result using the Windows code page, which
garbles UTF-8 text (`café` becomes `cafÃ©`). See the
[Contributing guide](./CONTRIBUTING.md) for what this means when changing
the driver.

`SQLGetData` returns text that doesn't fit the application's buffer in
parts. Each call returns the next part with SQLSTATE `01004`, the last part
returns `SQL_SUCCESS`, and a call after that returns `SQL_NO_DATA`. .NET
reads strings of 2047 or more characters this way.

## Building

Run these commands from a Visual Studio developer command prompt, opened
with `vcvars64.bat` for a 64-bit build or `vcvars32.bat` for a 32-bit one:

```
set VCPKG_ROOT=C:\path\to\vcpkg
cmake --preset x64-release
cmake --build out\build\x64-release
```

Set `VCPKG_ROOT` after running `vcvars*.bat`, not before. The Visual Studio
environment script sets its own `VCPKG_ROOT`, which replaces yours.

The build puts `TrinoODBC.dll` and the test executable `TestDriver.exe` in
`out\build\<preset>`. Every source file is listed explicitly in
`CMakeLists.txt`, so a new `.cpp` file must be added there, once for the
driver and once for the test executable, or it won't be built.

## Testing and Debugging

`TestDriver.exe` is a GoogleTest executable. Run it with no arguments to
run the whole suite, or pass `--gtest_filter` to run part of it:

```
out\build\x64-release\TestDriver.exe --gtest_filter=GetInfoTest.GetDriverVersion
out\build\x64-release\TestDriver.exe --gtest_filter=ValuePtrHelperTest.*
```

`TestDriver.exe` links the driver it was built with directly, so the tests
always exercise the code you just compiled. They do not go through the ODBC
Driver Manager or use whichever driver is registered on the machine.

Tests that talk to a server connect to a DSN named `TrinoTestRelease`, or
`TrinoTestDebug` in debug builds (see `test/constants.cpp`). The test suite
assumes this DSN points at a vanilla Trino instance, because tests query
`tpch.sf1.*`. Running a standalone copy of the
[Trino Docker Image](https://hub.docker.com/r/trinodb/trino) is enough.
The DSN only supplies connection settings, so it must be visible to the
test executable's bitness: create it with the 64-bit ODBC Data Source
Administrator for an x64 build, or the 32-bit one for an x86 build.

Some tests run without a server: the suites in `test/unit`, and the tests
that never run a query, such as most of `GetInfoTest`, `GetConnectAttrTest`
and `GetStmtAttrTest`. Tests that run a query fail if no server is
reachable.

To try the driver in an application such as Excel or Power BI, register
the build output DLL as a driver. ODBC drivers are installed entirely by
manipulating the Windows Registry. Set the keys as described in
`install/RegisterTrinoODBC.reg`, but substitute the path to the
`TrinoODBC.dll` file in the build output directory. Once this is done,
TrinoODBC shows up in the ODBC Data Sources window of the matching
bitness, and every build updates the driver the application uses. The
DSN setup dialog also comes from the registered driver, so you need a
registered driver to create the test DSN. Note that these registry keys
are the same ones used by the installed release driver, so following this
process replaces any installed driver with the one you built. To keep
both, edit `install/RegisterTrinoODBC.reg` to register the build under a
different driver name.


## Installing a Windows ODBC Driver

There are three ways to install this driver.

1. Using a pre-built and released MSI installer (See Releases Page)
2. Compiling your own driver and building your own MSI installer for it.
3. Manual driver compilation and installation.

### Using a pre-built MSI installer

See the releases page for the most recent release of this driver. Each release
publishes a 64-bit and a 32-bit installer named for that version, such as
`TrinoODBC_x64_0.0.8.msi`, along with a `SHA256SUMS.txt` file listing the hash
of each installer.

To confirm a download arrived intact, run
`Get-FileHash -Algorithm SHA256 .\TrinoODBC_x64_0.0.8.msi` in PowerShell and
compare the hash it prints against the matching line in `SHA256SUMS.txt`.
PowerShell prints the hash in upper case and `SHA256SUMS.txt` records it in
lower case, so compare the two without regard to case.

### Building your own MSI installer

This project uses [WiX](https://wixtoolset.org/) to build an installer package for
this driver.

1. We recommend using the .NET SDK to install WiX on your system. If developing in
   Visual Studio the .NET SDK is an optional installation option.
   You can re-run the installer and select "Modify" if you didn't select this the
   first time.
    1. If you want to manually install the .NET SDK, visit
       https://learn.microsoft.com/en-us/dotnet/core/sdk
1. If you haven't already done so, configure a nuget package source for the
   .NET SDK. Instructions for Visual Studio:
    1. https://stackoverflow.com/questions/69045231/nuget-package-sources-missing
1. Begin by installing the WiX tool on your system using the .NET SDK.
    1. Run `dotnet tool install --global wix --version 5.0.2`
1. Add the WiX UI extension.
    1. `wix extension add -g WixToolset.UI.wixext/5.0.2`
1. Build the installer
    1. `cd install`
    1. `./build_x64_installer.ps1` (for a 64-bit installer)
    1. `./build_x86_installer.ps1` (for a 32-bit installer)
1. The finished installer will appear, ready to be used. The filename carries
   the version, so a locally built 64-bit installer is named for whatever
   version was built, such as `TrinoODBC_x64_0.0.9.msi`. Builds that do not
   come from the release workflow use the default version in `CMakeLists.txt`,
   which is the next version expected to be released. Pass `-Version` to the
   script if you need a specific version instead.


### Manual Install

1. Open regedit as admin
2. Add Driver Information:
    1. Navigate to HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers.
    1. Right-click on ODBC Drivers, select New > String Value.
    1. Name the new string value with the name of your driver (e.g., TrinoODBC).
    1. Set its value to Installed.
3. Add Driver Configuration:
    1. Navigate to HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI.
    1. Right-click on ODBCINST.INI, select New > Key.
    1. Name the new key with the name of your driver (e.g., TrinoODBC).
    1. Under this new key, create the string values described in `install/RegisterTrinoODBC.reg`
    1. Driver: Full path to your driver DLL (e.g., C:\Path\To\TrinoODBC.dll).
    1. Setup: (optional) Full path to a setup DLL if you have one, or set it the same as the Driver.

# Dependencies

The following are upstream dependencies of the TrinoODBC driver. None of these dependencies
are included in source form in this repository - we recommend that you obtain them using the
vcpkg tool. All of them are used in unmodified form. Information about dependency
license distribution and compliance is also included below.

### Driver Runtime Dependencies

The driver's runtime dependencies are packaged and distributed when installing via the MSI
installer packages. Their licenses are available in `install\third_party_licenses.txt`, which is
automatically copied alongside the driver dll and the static libraries distributed with it.
After installation, you can find them at `[SystemFolder]\TrinoODBC\third_party_licenses.txt`
or `[System64Folder]\TrinoODBC\third_party_licenses.txt` depending on whether you are installing
a 32 or 64-bit driver.

1. [libcurl](https://curl.se/libcurl/)
    * [MIT License](https://github.com/Araq/libcurl/blob/master/LICENSE.txt)
1. [nlohmann JSON](https://github.com/nlohmann/json)
    * [MIT License](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT)
1. [openssl](https://www.openssl.org/)
    * [Apache 2.0 License](https://github.com/openssl/openssl/blob/master/LICENSE.txt)
1. [zlib](https://zlib.net/) (a dependency of libcurl, not this driver)
    * [zlib License](https://www.zlib.net/zlib_license.html)

### Specification Dependencies

1. [ODBC 3.8 Spec](https://learn.microsoft.com/en-us/sql/odbc/reference/what-s-new-in-odbc-3-8)
    * [MIT License](https://github.com/microsoft/ODBC-Specification/blob/master/license.txt)
    * Note: This is not distributed in source form with the TrinoODBC source
      nor in binary form with the installer. This license is mentioned for the
      purpose of transparency, not due to a legal requirement.

### Installer Dependencies

1. [WiX](https://wixtoolset.org/)
    * [Microsoft Reciprocal License (MS-RL)](https://wixtoolset.org/docs/about/)
    * Note: This license does not apply to the TrinoODBC driver because it is
      used solely to bundle/package the driver into an installer package. WiX
      itself is used in unmodified form.

### Testing Dependencies

1. [googletest (aka gtest)](https://github.com/google/googletest)
    * [BSD 3-Clause "Revised" License](https://github.com/google/googletest/blob/main/LICENSE)
    * Note: This is not distributed in source form with the TrinoODBC source
      nor in binary form with the installer. It is required to be provided
      by vcpkg if you wish to compile the test suite for this repository.
      This license is mentioned for the purpose of transparency, not due
      to a legal requirement.

## Development Resources

A treasure trove of API documentation for the ODBC API is available
[here](https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/odbc-api-reference?view=sql-server-ver16).

## Acknowledgements

Initial development of this driver was supported by
[Corteva Agriscience](https://www.corteva.com)
and released as open source under the Apache License 2.0.
We gratefully acknowledge their contribution to the Trino community.
