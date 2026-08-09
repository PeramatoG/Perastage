/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */

#include "apppaths.h"
#include "diagnostics/DiagnosticPaths.h"
#include "mvrimporter.h"
#include "mvrexporter.h"
#include "truss_gdtf_builder.h"

#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

// Returns false because MVR import is intentionally disabled in this stub.
bool MvrImporter::ImportAndRegister(const std::string &, bool, bool, ProgressCallback) {
  return false;
}

// Returns false because project-restore MVR import is intentionally disabled in this stub.
bool MvrImporter::ImportAndRegister(const std::string &, const MvrImportOptions &, ProgressCallback) {
  return false;
}

// Returns false because file-based MVR export is intentionally disabled in this stub.
bool MvrExporter::ExportToFile(const std::string &) {
  return false;
}

// Returns false because option-based MVR export is intentionally disabled in this stub.
bool MvrExporter::ExportToFile(const std::string &, const MvrExportOptions &) {
  return false;
}

// Returns false because buffer-based MVR export is intentionally disabled in this stub.
bool MvrExporter::ExportToBuffer(std::vector<unsigned char> &) {
  return false;
}

// Returns false because option-based buffer MVR export is intentionally disabled in this stub.
bool MvrExporter::ExportToBuffer(std::vector<unsigned char> &, const MvrExportOptions &) {
  return false;
}

// Returns collected export warnings for the stub exporter.
const std::vector<std::string> &MvrExporter::GetExportWarnings() const {
  return m_exportWarnings;
}

namespace AppPaths {

// Returns a deterministic temp directory for tests using app paths.
fs::path GetUserDataDir() {
  return fs::temp_directory_path() / "perastage_test_user_data";
}

// Returns a deterministic temp fallback directory for tests using app paths.
fs::path GetUserDataTempFallbackDir() {
  return fs::temp_directory_path() / "perastage_test_user_data_fallback";
}

// Returns the test user configuration file path.
fs::path GetUserConfigFilePath() {
  return GetUserDataDir() / "user_config.json";
}

// Returns the test log file path.
fs::path GetLogFilePath() {
  return GetUserDataDir() / "perastage.log";
}

// Ensures a test directory exists.
bool EnsureDirectory(const fs::path &dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  return !ec;
}

} // namespace AppPaths

namespace diagnostics {

// Returns the test logs directory.
fs::path DiagnosticPaths::LogsDirectory() { return AppPaths::GetUserDataDir() / "logs"; }

// Returns the test crash reports directory.
fs::path DiagnosticPaths::CrashReportsDirectory() { return AppPaths::GetUserDataDir() / "crashes"; }

// Returns the test reports directory.
fs::path DiagnosticPaths::ReportsDirectory() { return AppPaths::GetUserDataDir() / "reports"; }

// Returns the test current log path.
fs::path DiagnosticPaths::CurrentLogFile() { return LogsDirectory() / "perastage.log"; }

// Returns the test previous log path.
fs::path DiagnosticPaths::PreviousLogFile() { return LogsDirectory() / "perastage.previous.log"; }

// Returns the test crash report path.
fs::path DiagnosticPaths::NewCrashReportFile() { return CrashReportsDirectory() / "crash.txt"; }

// Returns the test diagnostic report path.
fs::path DiagnosticPaths::NewDiagnosticReportFile() { return ReportsDirectory() / "diagnostic.txt"; }

// Ensures a diagnostic test directory exists.
bool DiagnosticPaths::EnsureDirectory(const fs::path &directory, std::string *) {
  return AppPaths::EnsureDirectory(directory);
}

} // namespace diagnostics

// Returns false because legacy truss conversion is intentionally disabled in this stub.
bool ConvertLegacyGtrussToGdtf(const fs::path &, const fs::path &, std::string *) {
  return false;
}

// Returns false because truss GDTF generation is intentionally disabled in this stub.
bool BuildTrussGdtfFromInstance(const Truss &, const fs::path &, std::string *) {
  return false;
}

// Returns false because option-based truss GDTF generation is intentionally disabled in this stub.
bool BuildTrussGdtfFromInstance(const Truss &, const fs::path &, std::string *, const std::string &) {
  return false;
}
