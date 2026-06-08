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

#include <wx/platinfo.h>
#include <wx/utils.h>
#include <wx/version.h>

namespace diagnostics {
namespace {
std::mutex g_openGlMutex;
OpenGLInfo g_openGlInfo;

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

// Returns operating-system details available through wxWidgets.
std::string OperatingSystemDescription() {
  const wxPlatformInfo &platformInfo = wxPlatformInfo::Get();
  std::ostringstream stream;
  stream << ToUtf8(platformInfo.GetOperatingSystemDescription()) << " (";
  stream << ToUtf8(platformInfo.GetBitnessName()) << ')';
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
