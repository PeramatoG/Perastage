#include "diagnostics/DiagnosticLogger.h"
#include "filesystem_path_utils.h"

#include "diagnostics/DiagnosticPaths.h"
#include "logger.h"

#include <filesystem>

namespace diagnostics {
namespace {


} // namespace

// Initializes the underlying logger singleton.
void DiagnosticLogger::Initialize() { Logger::Instance(); }

// Writes an informational diagnostic message.
void DiagnosticLogger::Info(const std::string &message) {
  Logger::Instance().Log(Logger::Level::Info, message);
}

// Writes a warning diagnostic message.
void DiagnosticLogger::Warning(const std::string &message) {
  Logger::Instance().Log(Logger::Level::Warn, message);
}

// Writes an error diagnostic message.
void DiagnosticLogger::Error(const std::string &message) {
  Logger::Instance().Log(Logger::Level::Error, message);
}

// Writes a debug diagnostic message.
void DiagnosticLogger::Debug(const std::string &message) {
  Logger::Instance().Log(Logger::Level::Debug, message);
}

// Flushes queued diagnostic log messages to disk.
void DiagnosticLogger::Flush() { Logger::Instance().Flush(); }

// Stops diagnostic logging during application exit after a bounded final drain.
void DiagnosticLogger::ShutdownForExit(const std::string &finalMessage) {
  Logger::Instance().ShutdownForExit(finalMessage);
}

// Returns recent in-memory log lines for crash and diagnostic reports.
std::vector<std::string> DiagnosticLogger::RecentLines(std::size_t maxLines) {
  return Logger::Instance().GetRecentLines(maxLines);
}

// Returns the current persistent application log file path.
std::filesystem::path DiagnosticLogger::CurrentLogFile() {
  return DiagnosticPaths::CurrentLogFile();
}

// Reduces a potentially sensitive path to a filename for log breadcrumbs.
std::string DiagnosticLogger::FileNameOnly(const std::string &pathText) {
  if (pathText.empty())
    return {};
  std::filesystem::path path = PathUtils::PathFromUtf8(pathText);
  const std::u8string filenameU8 = path.filename().u8string();
  std::string filename(filenameU8.begin(), filenameU8.end());
  if (!filename.empty())
    return filename;
  return pathText;
}

} // namespace diagnostics
