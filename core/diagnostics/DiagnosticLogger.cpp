#include "diagnostics/DiagnosticLogger.h"

#include "diagnostics/DiagnosticPaths.h"
#include "logger.h"

#include <filesystem>

namespace diagnostics {
namespace {

// Converts UTF-8 text into a filesystem path without deprecated helpers.
std::filesystem::path PathFromUtf8(const std::string &text) {
  std::u8string u8Text;
  u8Text.reserve(text.size());
  for (const char ch : text)
    u8Text.push_back(static_cast<char8_t>(static_cast<unsigned char>(ch)));
  return std::filesystem::path(u8Text);
}

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
  std::filesystem::path path = PathFromUtf8(pathText);
  const std::u8string filenameU8 = path.filename().u8string();
  std::string filename(filenameU8.begin(), filenameU8.end());
  if (!filename.empty())
    return filename;
  return pathText;
}

} // namespace diagnostics
