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
#endif

namespace diagnostics {
namespace {
std::atomic_flag g_handlingCrash = ATOMIC_FLAG_INIT;

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

// Writes a crash report without involving GUI objects.
void WriteCrashReport(const std::string &reason, const std::string &details,
                      const std::string &stackTrace) {
  std::string directoryError;
  if (!DiagnosticPaths::EnsureDirectory(DiagnosticPaths::CrashReportsDirectory(),
                                        &directoryError)) {
    DiagnosticLogger::Error("Unable to create crash report directory: " +
                            directoryError);
    return;
  }

  const std::filesystem::path reportPath = DiagnosticPaths::NewCrashReportFile();
  std::ofstream out(reportPath, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    DiagnosticLogger::Error("Unable to write crash report file.");
    return;
  }
  out << DiagnosticReport::BuildTextReport(reason, details, stackTrace);
  out.close();
  DiagnosticLogger::Error("Crash report written: " + reportPath.string());
  DiagnosticLogger::Flush();
}

#if defined(_WIN32)
// Handles Windows structured exceptions by creating a local crash report.
LONG WINAPI HandleWindowsUnhandledException(EXCEPTION_POINTERS *exceptionInfo) {
  if (g_handlingCrash.test_and_set())
    return EXCEPTION_EXECUTE_HANDLER;

  std::ostringstream details;
  details << "Unhandled structured exception";
  if (exceptionInfo && exceptionInfo->ExceptionRecord) {
    details << "\nException code: 0x" << std::hex
            << exceptionInfo->ExceptionRecord->ExceptionCode;
    details << "\nException address: "
            << exceptionInfo->ExceptionRecord->ExceptionAddress;
  }
  WriteCrashReport("Unhandled structured exception", details.str(),
                   CaptureStackTrace());
  return EXCEPTION_EXECUTE_HANDLER;
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
