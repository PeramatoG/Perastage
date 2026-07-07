#include "diagnostics/DiagnosticReport.h"

#include "BuildInfo.h"
#include "diagnostics/DiagnosticLogger.h"
#include "diagnostics/DiagnosticPaths.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <wx/platinfo.h>
#include <wx/utils.h>
#include <wx/version.h>

namespace diagnostics {
namespace {
std::mutex g_openGlMutex;
OpenGLInfo g_openGlInfo;
std::mutex g_viewer2DCaptureMutex;
Viewer2DCaptureInfo g_viewer2DCaptureInfo;

// Formats a capture size for diagnostics without including scene data.
std::string FormatCaptureSize(int width, int height) {
  std::ostringstream stream;
  stream << width << 'x' << height;
  return stream.str();
}

// Returns true when a backend transition should be logged.
bool ShouldLogViewer2DBackendTransition(Viewer2DCaptureBackend previous,
                                        Viewer2DCaptureBackend current,
                                        std::uint64_t currentCount) {
  if (current == Viewer2DCaptureBackend::Fbo && currentCount == 1)
    return true;
  if (current == Viewer2DCaptureBackend::BackBufferFallback &&
      currentCount == 1)
    return true;
  return (previous == Viewer2DCaptureBackend::Fbo &&
          current == Viewer2DCaptureBackend::BackBufferFallback) ||
         (previous == Viewer2DCaptureBackend::BackBufferFallback &&
          current == Viewer2DCaptureBackend::Fbo);
}

// Converts a wxString value into UTF-8 text for reports.
std::string ToUtf8(const wxString &text) {
  const wxScopedCharBuffer utf8 = text.ToUTF8();
  return utf8 ? std::string(utf8.data()) : text.ToStdString();
}

// Returns a UTC timestamp for report headers.
std::string CurrentTimestampUtc() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm utcTime{};
#if defined(_WIN32)
  gmtime_s(&utcTime, &nowTime);
#else
  gmtime_r(&nowTime, &utcTime);
#endif
  std::ostringstream stream;
  stream << std::put_time(&utcTime, "%Y-%m-%d %H:%M:%S UTC");
  return stream.str();
}

// Returns a wxWidgets version string compiled into this build.
std::string WxVersionString() {
  std::ostringstream stream;
  stream << wxMAJOR_VERSION << '.' << wxMINOR_VERSION << '.' << wxRELEASE_NUMBER;
  return stream.str();
}

#if defined(_WIN32)
// Returns the native Windows version without relying on compatibility manifests.
std::string NativeWindowsVersionDescription() {
  using RtlGetVersionFn = LONG(WINAPI *)(OSVERSIONINFOW *);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll)
    return {};

  auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
      GetProcAddress(ntdll, "RtlGetVersion"));
  if (!rtlGetVersion)
    return {};

  OSVERSIONINFOW versionInfo{};
  versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
  if (rtlGetVersion(&versionInfo) != 0)
    return {};

  std::ostringstream stream;
  stream << "Windows native " << versionInfo.dwMajorVersion << '.'
         << versionInfo.dwMinorVersion << " build "
         << versionInfo.dwBuildNumber;
  return stream.str();
}
#endif

// Returns operating-system details available through wxWidgets and native APIs.
std::string OperatingSystemDescription() {
  const wxPlatformInfo &platformInfo = wxPlatformInfo::Get();
  std::ostringstream stream;
  stream << ToUtf8(platformInfo.GetOperatingSystemDescription()) << " (";
  stream << ToUtf8(platformInfo.GetBitnessName()) << ')';
#if defined(_WIN32)
  const std::string nativeVersion = NativeWindowsVersionDescription();
  if (!nativeVersion.empty())
    stream << "; " << nativeVersion;
#endif
  return stream.str();
}

// Appends build and runtime metadata to a diagnostic text stream.
void AppendSystemInfo(std::ostringstream &stream) {
  stream << "Perastage version: " << build_info::kVersionDisplay << '\n';
  stream << "Git commit: " << build_info::kGitCommit << '\n';
  stream << "Build timestamp: " << build_info::kBuildTimestampUtc << '\n';
  stream << "Build type: " << build_info::kBuildType << '\n';
  stream << "Target platform: " << build_info::kTargetPlatform << '\n';
  stream << "Compiler: " << build_info::kCompiler << '\n';
  stream << "Operating system: " << OperatingSystemDescription() << '\n';
  stream << "wxWidgets version: " << WxVersionString() << '\n';

  const OpenGLInfo openGlInfo = DiagnosticReport::GetOpenGLInfo();
  if (!openGlInfo.vendor.empty() || !openGlInfo.renderer.empty() ||
      !openGlInfo.version.empty()) {
    stream << "OpenGL vendor: " << openGlInfo.vendor << '\n';
    stream << "OpenGL renderer: " << openGlInfo.renderer << '\n';
    stream << "OpenGL version: " << openGlInfo.version << '\n';
  } else {
    stream << "OpenGL: not captured yet\n";
  }
}

// Appends Viewer2D RGBA capture diagnostics to a diagnostic text stream.
void AppendViewer2DCaptureInfo(std::ostringstream &stream) {
  const Viewer2DCaptureInfo captureInfo =
      DiagnosticReport::GetViewer2DCaptureInfo();
  stream << "Viewer2D RGBA capture backend: "
         << DiagnosticReport::Viewer2DCaptureBackendName(captureInfo.backend)
         << '\n';
  stream << "Viewer2D FBO captures: " << captureInfo.fboSuccessCount << '\n';
  stream << "Viewer2D fallback captures: " << captureInfo.fallbackCount << '\n';
  stream << "Viewer2D capture failures: " << captureInfo.failureCount << '\n';
  if (captureInfo.backend == Viewer2DCaptureBackend::NotUsed) {
    stream << "Viewer2D last capture size: not used\n";
  } else {
    stream << "Viewer2D last capture size: "
           << FormatCaptureSize(captureInfo.lastWidth, captureInfo.lastHeight)
           << '\n';
  }
  stream << "Viewer2D fallback ever used: "
         << (captureInfo.fallbackEverUsed ? "yes" : "no") << '\n';
  stream << "Viewer2D last capture diagnostic: "
         << (captureInfo.lastDiagnostic.empty() ? "none"
                                                : captureInfo.lastDiagnostic)
         << '\n';
}

} // namespace

// Stores OpenGL driver details after a valid context is initialized.
void DiagnosticReport::SetOpenGLInfo(OpenGLInfo info) {
  std::lock_guard<std::mutex> lock(g_openGlMutex);
  g_openGlInfo = std::move(info);
}

// Returns the latest captured OpenGL driver details.
OpenGLInfo DiagnosticReport::GetOpenGLInfo() {
  std::lock_guard<std::mutex> lock(g_openGlMutex);
  return g_openGlInfo;
}

// Records a successful Viewer2D RGBA capture through the FBO path.
void DiagnosticReport::RecordViewer2DFboCapture(int width, int height) {
  bool shouldLog = false;
  {
    std::lock_guard<std::mutex> lock(g_viewer2DCaptureMutex);
    const Viewer2DCaptureBackend previous = g_viewer2DCaptureInfo.backend;
    g_viewer2DCaptureInfo.backend = Viewer2DCaptureBackend::Fbo;
    ++g_viewer2DCaptureInfo.fboSuccessCount;
    g_viewer2DCaptureInfo.lastWidth = width;
    g_viewer2DCaptureInfo.lastHeight = height;
    g_viewer2DCaptureInfo.lastDiagnostic.clear();
    shouldLog = ShouldLogViewer2DBackendTransition(
        previous, Viewer2DCaptureBackend::Fbo,
        g_viewer2DCaptureInfo.fboSuccessCount);
  }
  if (shouldLog) {
    DiagnosticLogger::Info("Viewer2D RGBA capture backend=FBO size=" +
                           FormatCaptureSize(width, height));
  }
}

// Records a Viewer2D RGBA capture through the GL_BACK fallback path.
void DiagnosticReport::RecordViewer2DBackBufferFallback(
    int width, int height, const std::string &reason) {
  bool shouldLog = false;
  {
    std::lock_guard<std::mutex> lock(g_viewer2DCaptureMutex);
    const Viewer2DCaptureBackend previous = g_viewer2DCaptureInfo.backend;
    g_viewer2DCaptureInfo.backend = Viewer2DCaptureBackend::BackBufferFallback;
    ++g_viewer2DCaptureInfo.fallbackCount;
    g_viewer2DCaptureInfo.lastWidth = width;
    g_viewer2DCaptureInfo.lastHeight = height;
    g_viewer2DCaptureInfo.lastDiagnostic = reason;
    g_viewer2DCaptureInfo.fallbackEverUsed = true;
    shouldLog = ShouldLogViewer2DBackendTransition(
        previous, Viewer2DCaptureBackend::BackBufferFallback,
        g_viewer2DCaptureInfo.fallbackCount);
  }
  if (shouldLog) {
    DiagnosticLogger::Warning(
        "Viewer2D RGBA capture backend=GL_BACK fallback size=" +
        FormatCaptureSize(width, height) + " reason=" +
        (reason.empty() ? "unspecified" : reason));
  }
}

// Records a definitive Viewer2D RGBA capture failure.
void DiagnosticReport::RecordViewer2DCaptureFailure(
    int width, int height, const std::string &reason) {
  std::lock_guard<std::mutex> lock(g_viewer2DCaptureMutex);
  g_viewer2DCaptureInfo.backend = Viewer2DCaptureBackend::Failed;
  ++g_viewer2DCaptureInfo.failureCount;
  g_viewer2DCaptureInfo.lastWidth = width;
  g_viewer2DCaptureInfo.lastHeight = height;
  g_viewer2DCaptureInfo.lastDiagnostic = reason;
}

// Returns the latest Viewer2D RGBA capture diagnostics.
Viewer2DCaptureInfo DiagnosticReport::GetViewer2DCaptureInfo() {
  std::lock_guard<std::mutex> lock(g_viewer2DCaptureMutex);
  return g_viewer2DCaptureInfo;
}

// Converts a Viewer2D RGBA capture backend to stable report text.
std::string DiagnosticReport::Viewer2DCaptureBackendName(
    Viewer2DCaptureBackend backend) {
  switch (backend) {
  case Viewer2DCaptureBackend::NotUsed:
    return "Not used";
  case Viewer2DCaptureBackend::Fbo:
    return "FBO";
  case Viewer2DCaptureBackend::BackBufferFallback:
    return "GL_BACK fallback";
  case Viewer2DCaptureBackend::Failed:
    return "Failed";
  }
  return "Unknown";
}

// Builds a complete plain-text diagnostic report for crashes or manual export.
std::string DiagnosticReport::BuildTextReport(const std::string &reason,
                                              const std::string &eventDetails,
                                              const std::string &stackTrace) {
  std::ostringstream stream;
  stream << "Perastage Diagnostic Report\n";
  stream << "===========================\n\n";
  stream << "This file contains local diagnostic information. It is never sent "
            "automatically and can be shared manually with the developer.\n\n";
  stream << "Timestamp: " << CurrentTimestampUtc() << '\n';
  stream << "Reason: " << (reason.empty() ? "Manual diagnostic export" : reason)
         << "\n\n";

  stream << "Build and system information\n";
  stream << "----------------------------\n";
  AppendSystemInfo(stream);

  stream << "\nViewer2D capture diagnostics\n";
  stream << "----------------------------\n";
  AppendViewer2DCaptureInfo(stream);

  if (!eventDetails.empty()) {
    stream << "\nEvent details\n";
    stream << "-------------\n";
    stream << eventDetails << "\n";
  }

  if (!stackTrace.empty()) {
    stream << "\nStack trace\n";
    stream << "-----------\n";
    stream << stackTrace << "\n";
  }

  DiagnosticLogger::Flush();
  stream << "\nRecent log lines\n";
  stream << "----------------\n";
  const std::vector<std::string> recentLines = DiagnosticLogger::RecentLines(240);
  if (recentLines.empty()) {
    stream << "No recent log lines are available.\n";
  } else {
    for (const std::string &line : recentLines)
      stream << line << '\n';
  }
  return stream.str();
}

// Exports a manual diagnostic report and returns the generated file path.
bool DiagnosticReport::ExportToFile(std::filesystem::path *outPath,
                                    std::string *error) {
  std::string directoryError;
  if (!DiagnosticPaths::EnsureDirectory(DiagnosticPaths::ReportsDirectory(),
                                        &directoryError)) {
    if (error)
      *error = directoryError;
    return false;
  }

  const std::filesystem::path reportPath =
      DiagnosticPaths::NewDiagnosticReportFile();
  std::ofstream out(reportPath, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    if (error)
      *error = "Unable to create diagnostic report file.";
    return false;
  }
  out << BuildTextReport("Manual diagnostic export");
  out.close();
  if (outPath)
    *outPath = reportPath;
  return true;
}

} // namespace diagnostics
