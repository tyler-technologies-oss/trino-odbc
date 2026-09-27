#include "browserInteraction.hpp"
#include <shellapi.h>

#include "writeLog.hpp"

/*
Whether a user can see windows this process opens. A driver loaded by a
Windows service, such as SQL Server running a linked server query, runs
in session 0 or on a hidden window station. A browser or message box
opened there never appears, and nobody can close it.
*/
static bool hasVisibleDesktop() {
  DWORD sessionId = 0;
  if (not ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) or
      sessionId == 0) {
    return false;
  }

  HWINSTA windowStation = GetProcessWindowStation();
  USEROBJECTFLAGS flags = {};
  if (windowStation == nullptr or
      not GetUserObjectInformation(
          windowStation, UOI_FLAGS, &flags, sizeof(flags), nullptr)) {
    return false;
  }
  return (flags.dwFlags & WSF_VISIBLE) != 0;
}

bool openURLInDefaultBrowser(const std::string& url) {
  if (not hasVisibleDesktop()) {
    WriteLog(LL_ERROR,
             "  ERROR: Can't open a browser to log in, because this "
             "process has no visible desktop. It is probably running as a "
             "service.");
    return false;
  }

  HINSTANCE result =
      ShellExecute(nullptr, nullptr, url.c_str(), nullptr, nullptr, SW_SHOW);

  if (reinterpret_cast<intptr_t>(result) <= 32) {
    WriteLog(LL_ERROR, "  ERROR: Failed to open the default browser");
    MessageBox(nullptr,
               "Failed to open default browser",
               "Authentication Error",
               MB_ICONERROR);
    return false;
  }
  return true;
}
