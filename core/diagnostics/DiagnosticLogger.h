#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Logger;

namespace diagnostics {

// Provides application-level logging helpers for diagnostics and breadcrumbs.
class DiagnosticLogger {
public:
  static void Initialize();
  static void Info(const std::string &message);
  static void Warning(const std::string &message);
  static void Error(const std::string &message);
  static void Debug(const std::string &message);
  static void Flush();
  static std::vector<std::string> RecentLines(std::size_t maxLines);
  static std::filesystem::path CurrentLogFile();
  static std::string FileNameOnly(const std::string &pathText);
};

} // namespace diagnostics
