#pragma once

#include <filesystem>
#include <string>

namespace diagnostics {

// Provides per-user filesystem locations for logs and crash reports.
class DiagnosticPaths {
public:
  static std::filesystem::path LogsDirectory();
  static std::filesystem::path CrashReportsDirectory();
  static std::filesystem::path ReportsDirectory();
  static std::filesystem::path CurrentLogFile();
  static std::filesystem::path PreviousLogFile();
  static std::filesystem::path NewCrashReportFile();
  static std::filesystem::path NewDiagnosticReportFile();
  static bool EnsureDirectory(const std::filesystem::path &directory,
                              std::string *error = nullptr);
};

} // namespace diagnostics
