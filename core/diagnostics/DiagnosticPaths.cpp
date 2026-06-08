#include "diagnostics/DiagnosticPaths.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>

#ifdef __APPLE__
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace diagnostics {
namespace {

// Returns the current local time formatted for diagnostic filenames.
std::string TimestampForFilename() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm localTime{};
#if defined(_WIN32)
  localtime_s(&localTime, &nowTime);
#else
  localtime_r(&nowTime, &localTime);
#endif
  std::ostringstream stream;
  stream << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S");
  return stream.str();
}

// Converts UTF-8 text into a filesystem path without deprecated helpers.
std::filesystem::path PathFromUtf8(const std::string &text) {
  std::u8string u8Text;
  u8Text.reserve(text.size());
  for (const char ch : text)
    u8Text.push_back(static_cast<char8_t>(static_cast<unsigned char>(ch)));
  return std::filesystem::path(u8Text);
}

// Returns a home directory path from environment or platform user data.
std::filesystem::path HomeDirectory() {
#if defined(_WIN32)
  if (const char *userProfile = std::getenv("USERPROFILE")) {
    if (*userProfile)
      return PathFromUtf8(userProfile);
  }
#else
  if (const char *home = std::getenv("HOME")) {
    if (*home)
      return PathFromUtf8(home);
  }
#ifdef __APPLE__
  if (const passwd *pw = getpwuid(getuid())) {
    if (pw->pw_dir && *pw->pw_dir)
      return PathFromUtf8(pw->pw_dir);
  }
#endif
#endif
  return std::filesystem::temp_directory_path() / "perastage";
}

// Resolves the platform-specific base directory for Perastage diagnostics.
std::filesystem::path DiagnosticsBaseDirectory() {
#if defined(_WIN32)
  if (const char *localAppData = std::getenv("LOCALAPPDATA")) {
    if (*localAppData)
      return PathFromUtf8(localAppData) / "Perastage";
  }
  return HomeDirectory() / "AppData" / "Local" / "Perastage";
#elif defined(__APPLE__)
  return HomeDirectory() / "Library" / "Logs" / "Perastage";
#else
  if (const char *xdgStateHome = std::getenv("XDG_STATE_HOME")) {
    if (*xdgStateHome)
      return PathFromUtf8(xdgStateHome) / "perastage";
  }
  return HomeDirectory() / ".local" / "state" / "perastage";
#endif
}

} // namespace

// Returns the per-user directory that stores persistent log files.
std::filesystem::path DiagnosticPaths::LogsDirectory() {
#if defined(__APPLE__)
  return DiagnosticsBaseDirectory();
#else
  return DiagnosticsBaseDirectory() / "logs";
#endif
}

// Returns the per-user directory that stores local crash reports.
std::filesystem::path DiagnosticPaths::CrashReportsDirectory() {
  return LogsDirectory() / "crash_reports";
}

// Returns the per-user directory that stores manually exported diagnostic reports.
std::filesystem::path DiagnosticPaths::ReportsDirectory() {
  return LogsDirectory() / "reports";
}

// Returns the latest application log file path.
std::filesystem::path DiagnosticPaths::CurrentLogFile() {
  return LogsDirectory() / "perastage.log";
}

// Returns the previous application log file path kept during rotation.
std::filesystem::path DiagnosticPaths::PreviousLogFile() {
  return LogsDirectory() / "perastage.previous.log";
}

// Returns a unique crash report path for the current local timestamp.
std::filesystem::path DiagnosticPaths::NewCrashReportFile() {
  return CrashReportsDirectory() /
         ("PerastageCrashReport_" + TimestampForFilename() + ".txt");
}

// Returns a unique manual diagnostic report path for the current local timestamp.
std::filesystem::path DiagnosticPaths::NewDiagnosticReportFile() {
  return ReportsDirectory() /
         ("PerastageDiagnosticReport_" + TimestampForFilename() + ".txt");
}

// Ensures a diagnostics directory exists and optionally returns an error string.
bool DiagnosticPaths::EnsureDirectory(const std::filesystem::path &directory,
                                      std::string *error) {
  if (directory.empty()) {
    if (error)
      *error = "Diagnostics directory is empty.";
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories(directory, ec);
  if (ec && error)
    *error = ec.message();
  return !ec;
}

} // namespace diagnostics
