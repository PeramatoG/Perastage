#include "diagnostics/CrashHandler.h"

#include "diagnostics/DiagnosticLogger.h"
#include "diagnostics/DiagnosticPaths.h"
#include "diagnostics/DiagnosticReport.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <backward.hpp>

#if defined(_WIN32)
#include <windows.h>
#include <minidumpapiset.h>
#include <iomanip>
#endif

namespace diagnostics {
namespace {
std::atomic_flag g_handlingCrash = ATOMIC_FLAG_INIT;
#if defined(_WIN32)
void *g_vectoredExceptionHandler = nullptr;
#endif

// Returns a human-readable signal name for crash reports.
const char *SignalName(int signalNumber) {
  switch (signalNumber) {
  case SIGABRT:
    return "SIGABRT";
  case SIGFPE:
    return "SIGFPE";
  case SIGILL:
    return "SIGILL";
  case SIGSEGV:
    return "SIGSEGV";
#ifdef SIGBUS
  case SIGBUS:
    return "SIGBUS";
#endif
#ifdef SIGTRAP
  case SIGTRAP:
    return "SIGTRAP";
#endif
  default:
    return "Unknown signal";
  }
}

// Captures a best-effort stack trace using backward-cpp.
std::string CaptureStackTrace(std::size_t depth = 64) {
  backward::StackTrace stackTrace;
  stackTrace.load_here(depth);
  backward::Printer printer;
  printer.object = true;
  printer.color_mode = backward::ColorMode::never;
  printer.address = true;
  printer.snippet = false;
  std::ostringstream stream;
  printer.print(stackTrace, stream);
  return stream.str();
}

#if defined(_WIN32)
// Writes a Windows minidump file next to the text crash report.
bool WriteWindowsMinidump(const std::filesystem::path &dumpPath,
                          EXCEPTION_POINTERS *exceptionInfo,
                          std::string &error) {
  HANDLE file = CreateFileW(dumpPath.wstring().c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    error = "CreateFileW failed with error " + std::to_string(GetLastError());
    return false;
  }

  MINIDUMP_EXCEPTION_INFORMATION exceptionParam{};
  exceptionParam.ThreadId = GetCurrentThreadId();
  exceptionParam.ExceptionPointers = exceptionInfo;
  exceptionParam.ClientPointers = FALSE;

  const BOOL ok = MiniDumpWriteDump(
      GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal,
      exceptionInfo ? &exceptionParam : nullptr, nullptr, nullptr);
  if (!ok)
    error = "MiniDumpWriteDump failed with error " + std::to_string(GetLastError());

  CloseHandle(file);
  return ok == TRUE;
}

// Returns true when a Windows exception represents a serious native crash.
bool IsSeriousWindowsException(DWORD exceptionCode) {
  switch (exceptionCode) {
  case EXCEPTION_ACCESS_VIOLATION:
  case EXCEPTION_ILLEGAL_INSTRUCTION:
  case EXCEPTION_STACK_OVERFLOW:
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
  case EXCEPTION_DATATYPE_MISALIGNMENT:
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:
  case EXCEPTION_INT_DIVIDE_BY_ZERO:
    return true;
  default:
    return false;
  }
}

// Resolves the module path that contains a faulting Windows instruction address.
std::string ResolveFaultingModulePath(void *address) {
  if (!address)
    return {};

  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(address), &module)) {
    return {};
  }

  wchar_t path[MAX_PATH] = {};
  const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
  if (length == 0)
    return {};

  const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0,
                                            nullptr, nullptr);
  if (utf8Length <= 0)
    return {};

  std::string utf8(static_cast<std::size_t>(utf8Length - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8.data(), utf8Length, nullptr,
                      nullptr);
  return utf8;
}

// Appends Windows exception metadata to crash report details.
void AppendWindowsExceptionDetails(std::ostringstream &details,
                                   EXCEPTION_POINTERS *exceptionInfo) {
  if (!exceptionInfo || !exceptionInfo->ExceptionRecord) {
    details << "\nWindows exception context: unavailable";
    return;
  }

  EXCEPTION_RECORD *record = exceptionInfo->ExceptionRecord;
  details << "\nWindows exception context: available";
  details << "\nException code: 0x" << std::hex << std::setw(8)
          << std::setfill('0') << record->ExceptionCode << std::dec;
  details << "\nException address: " << record->ExceptionAddress;
  const std::string modulePath =
      ResolveFaultingModulePath(record->ExceptionAddress);
  if (!modulePath.empty())
    details << "\nFaulting module: " << modulePath;
}

// Writes a Windows crash report after an already-attempted native minidump capture.
void WriteWindowsExceptionCrashReport(const std::filesystem::path &reportPath,
                                      const std::filesystem::path &dumpPath,
                                      bool dumpWritten,
                                      const std::string &dumpError,
                                      EXCEPTION_POINTERS *exceptionInfo) {
  std::ostringstream details;
  details << "Unhandled structured exception";
  AppendWindowsExceptionDetails(details, exceptionInfo);
  details << "\nMinidump exception context: "
          << (exceptionInfo ? "real Windows exception context" : "unavailable");
  if (dumpWritten) {
    details << "\nWindows minidump: " << dumpPath.string();
    DiagnosticLogger::Error("Windows minidump written: " + dumpPath.string());
  } else {
    details << "\nWindows minidump unavailable: " << dumpError;
    DiagnosticLogger::Error("Unable to write Windows minidump: " + dumpError);
  }
  details << "\nText stack trace: best-effort backward-cpp capture";

  std::ofstream out(reportPath, std::ios::out | std::ios::trunc);
  if (out.is_open()) {
    out << DiagnosticReport::BuildTextReport(
        "Unhandled structured exception", details.str(), CaptureStackTrace());
    out.close();
    DiagnosticLogger::Error("Crash report written: " + reportPath.string());
  } else {
    DiagnosticLogger::Error("Unable to write crash report file.");
  }
  DiagnosticLogger::Flush();
}
#endif

// Writes a crash report without involving GUI objects.
void WriteCrashReport(const std::string &reason, const std::string &details,
                      const std::string &stackTrace,
                      void *exceptionInfo = nullptr) {
  std::string directoryError;
  if (!DiagnosticPaths::EnsureDirectory(DiagnosticPaths::CrashReportsDirectory(),
                                        &directoryError)) {
    DiagnosticLogger::Error("Unable to create crash report directory: " +
                            directoryError);
    return;
  }

  const std::filesystem::path reportPath = DiagnosticPaths::NewCrashReportFile();
  std::string enrichedDetails = details;
#if defined(_WIN32)
  const std::filesystem::path dumpPath = reportPath.parent_path() /
                                        reportPath.stem().concat(".dmp");
  std::string dumpError;
  if (WriteWindowsMinidump(
          dumpPath, static_cast<EXCEPTION_POINTERS *>(exceptionInfo),
          dumpError)) {
    enrichedDetails += enrichedDetails.empty() ? "" : "\n";
    enrichedDetails += "Windows minidump: " + dumpPath.string();
    enrichedDetails += "\nMinidump exception context: " +
                       std::string(exceptionInfo ? "real Windows exception context"
                                                 : "unavailable");
    DiagnosticLogger::Error("Windows minidump written: " + dumpPath.string());
  } else {
    enrichedDetails += enrichedDetails.empty() ? "" : "\n";
    enrichedDetails += "Windows minidump unavailable: " + dumpError;
    DiagnosticLogger::Error("Unable to write Windows minidump: " + dumpError);
  }
#else
  (void)exceptionInfo;
#endif

  std::ofstream out(reportPath, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    DiagnosticLogger::Error("Unable to write crash report file.");
    return;
  }
  out << DiagnosticReport::BuildTextReport(reason, enrichedDetails, stackTrace);
  out.close();
  DiagnosticLogger::Error("Crash report written: " + reportPath.string());
  DiagnosticLogger::Flush();
}

#if defined(_WIN32)
// Handles a serious Windows exception by writing a dump before text stack capture.
LONG HandleWindowsException(EXCEPTION_POINTERS *exceptionInfo, LONG handledResult) {
  if (g_handlingCrash.test_and_set())
    return handledResult;

  if (!exceptionInfo || !exceptionInfo->ExceptionRecord ||
      !IsSeriousWindowsException(exceptionInfo->ExceptionRecord->ExceptionCode)) {
    g_handlingCrash.clear();
    return EXCEPTION_CONTINUE_SEARCH;
  }

  std::string directoryError;
  if (!DiagnosticPaths::EnsureDirectory(DiagnosticPaths::CrashReportsDirectory(),
                                        &directoryError)) {
    DiagnosticLogger::Error("Unable to create crash report directory: " +
                            directoryError);
    DiagnosticLogger::Flush();
    return handledResult;
  }

  const std::filesystem::path reportPath = DiagnosticPaths::NewCrashReportFile();
  const std::filesystem::path dumpPath = reportPath.parent_path() /
                                        reportPath.stem().concat(".dmp");
  std::string dumpError;
  const bool dumpWritten = WriteWindowsMinidump(dumpPath, exceptionInfo, dumpError);
  WriteWindowsExceptionCrashReport(reportPath, dumpPath, dumpWritten, dumpError,
                                   exceptionInfo);
  return handledResult;
}

// Handles Windows structured exceptions by creating a local crash report.
LONG WINAPI HandleWindowsUnhandledException(EXCEPTION_POINTERS *exceptionInfo) {
  return HandleWindowsException(exceptionInfo, EXCEPTION_EXECUTE_HANDLER);
}

// Handles first-chance Windows exceptions before CRT signal translation can lose context.
LONG WINAPI HandleWindowsVectoredException(EXCEPTION_POINTERS *exceptionInfo) {
  return HandleWindowsException(exceptionInfo, EXCEPTION_CONTINUE_SEARCH);
}
#endif

// Handles fatal POSIX/CRT signals by creating a local crash report.
void HandleSignal(int signalNumber) {
  if (g_handlingCrash.test_and_set())
    std::_Exit(128 + signalNumber);

  std::ostringstream details;
  details << "Fatal signal: " << SignalName(signalNumber) << " ("
          << signalNumber << ')';
  WriteCrashReport("Fatal signal", details.str(), CaptureStackTrace());

  std::signal(signalNumber, SIG_DFL);
  std::raise(signalNumber);
  std::_Exit(128 + signalNumber);
}

// Handles std::terminate paths by creating a local crash report before aborting.
void HandleTerminate() {
  if (g_handlingCrash.test_and_set())
    std::_Exit(EXIT_FAILURE);

  WriteCrashReport("Unhandled C++ exception or terminate", {}, CaptureStackTrace());
  std::abort();
}

// Installs process signal handlers for fatal crashes.
void InstallSignalHandlers() {
  std::signal(SIGABRT, HandleSignal);
  std::signal(SIGFPE, HandleSignal);
  std::signal(SIGILL, HandleSignal);
  std::signal(SIGSEGV, HandleSignal);
#ifdef SIGBUS
  std::signal(SIGBUS, HandleSignal);
#endif
#ifdef SIGTRAP
  std::signal(SIGTRAP, HandleSignal);
#endif
}

} // namespace

// Installs backward-cpp backed crash handling for this process.
void CrashHandler::Initialize() {
  static const backward::SignalHandling backwardSignalHandling;
  (void)backwardSignalHandling;
  InstallSignalHandlers();
#if defined(_WIN32)
  if (!g_vectoredExceptionHandler)
    g_vectoredExceptionHandler =
        AddVectoredExceptionHandler(1, HandleWindowsVectoredException);
  SetUnhandledExceptionFilter(HandleWindowsUnhandledException);
#endif
  std::set_terminate(HandleTerminate);
  DiagnosticLogger::Info("Crash handler initialized.");
}

// Writes a crash-style report for wxWidgets exception hooks.
void CrashHandler::WriteExceptionReport(const std::string &reason,
                                        const std::string &details) {
  WriteCrashReport(reason, details, CaptureStackTrace());
}

} // namespace diagnostics
