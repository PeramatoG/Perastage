// Configures Windows test processes to report crashes and assertions to stderr.
#if defined(_WIN32)
#include <windows.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#include <cstdlib>
#endif

namespace {

struct WindowsNoDialogRuntime {
  // Installs non-modal Windows and CRT failure reporting for test executables.
  WindowsNoDialogRuntime() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
#if defined(_MSC_VER)
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  }
};

const WindowsNoDialogRuntime kWindowsNoDialogRuntime;

} // namespace
#endif
