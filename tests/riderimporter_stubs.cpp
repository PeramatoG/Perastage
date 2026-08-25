/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
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
#include <string>
#include <filesystem>
#include <optional>
#include <unordered_map>

#include "gdtfdictionary.h"
#include "trussdictionary.h"
#include "trussloader.h"
#include "truss_gdtf_builder.h"
#include "pdftext.h"
#include "mvrimporter.h"
#include "mvrexporter.h"
#include "truss.h"

// Returns an empty string because PDF extraction is intentionally stubbed.
std::string ExtractPdfText(const std::string &) { return {}; }

namespace GdtfDictionary {
// Returns an empty dictionary because loading is intentionally stubbed.
std::optional<std::unordered_map<std::string, Entry>> Load() { return std::unordered_map<std::string, Entry>(); }
// Returns true to simulate successful dictionary persistence in tests.
bool Save(const std::unordered_map<std::string, Entry> &, std::string *) { return true; }
// Returns no entry because lookup is intentionally stubbed.
std::optional<Entry> Get(const std::string &) { return std::nullopt; }
// Returns no entry because loaded-dictionary lookup is intentionally stubbed.
std::optional<Entry> FindInLoadedDictionary(
    const std::unordered_map<std::string, Entry> &, const std::string &, bool) {
  return std::nullopt;
}
// Performs no operation because dictionary updates are intentionally stubbed.
void Update(const std::string &, const std::string &, const std::string &, const std::string &) {}
// Performs no operation because category updates are intentionally stubbed.
void UpdateCategory(const std::string &, const std::string &) {}
// Performs no operation because bulk category updates are intentionally stubbed.
void UpdateCategoriesBulk(const std::unordered_map<std::string, std::string> &) {}
// Returns zero because save-call tracking is intentionally stubbed.
size_t GetSaveCallCountForTesting() { return 0; }
// Performs no operation because save-call reset is intentionally stubbed.
void ResetSaveCallCountForTesting() {}
}

namespace TrussDictionary {
// Returns the input model unchanged because normalization is intentionally stubbed.
std::string NormalizeModelKey(const std::string &model) { return model; }
// Returns an empty dictionary because loading is intentionally stubbed.
std::optional<std::unordered_map<std::string, std::string>> Load() { return std::unordered_map<std::string, std::string>(); }
// Performs no operation because dictionary saving is intentionally stubbed.
void Save(const std::unordered_map<std::string, std::string> &) {}
// Returns no value because lookup is intentionally stubbed.
std::optional<std::string> Get(const std::string &) { return std::nullopt; }
// Returns no value because loaded-dictionary lookup is intentionally stubbed.
std::optional<std::string> FindInLoadedDictionary(
    const std::unordered_map<std::string, std::string> &, const std::string &,
    bool) {
  return std::nullopt;
}
// Performs no operation because dictionary updates are intentionally stubbed.
void Update(const std::string &, const std::string &) {}
// Returns false because truss-file import is intentionally disabled in this stub.
bool ImportTrussFile(const std::string &, std::string &, std::string &) { return false; }
}

// Returns false because truss-archive loading is intentionally disabled in this stub.
bool LoadTrussArchive(const std::string &, Truss &) { return false; }

// Returns false because file-based MVR import is intentionally disabled in this stub.
bool MvrImporter::ImportFromFile(const std::string &, bool, bool, ProgressCallback) { return false; }
// Returns false because register-based MVR import is intentionally disabled in this stub.
bool MvrImporter::ImportAndRegister(const std::string &, bool, bool, ProgressCallback) { return false; }
// Returns false because option-based MVR import is intentionally disabled in this stub.
bool MvrImporter::ImportAndRegister(const std::string &,
                                    const MvrImportOptions &,
                                    ProgressCallback) {
  return false;
}
// Returns false because buffer-based project-restore MVR import is disabled in this stub.
bool MvrImporter::ImportAndRegisterFromBuffer(
    const std::vector<std::uint8_t> &, const MvrImportOptions &,
    ProgressCallback) {
  return false;
}
// Returns false because file-based MVR export is intentionally disabled in this stub.
bool MvrExporter::ExportToFile(const std::string &) { return false; }
// Returns false because buffer-based MVR export is intentionally disabled in this stub.
bool MvrExporter::ExportToBuffer(std::vector<unsigned char> &) { return false; }
// Returns false because option-based buffer MVR export is intentionally disabled in this stub.
bool MvrExporter::ExportToBuffer(std::vector<unsigned char> &,
                                  const MvrExportOptions &) {
  return false;
}
// Returns collected export warnings for the stub exporter.
const std::vector<std::string> &MvrExporter::GetExportWarnings() const {
  return m_exportWarningAdapter;
}

const std::vector<MvrExportDiagnostic> &MvrExporter::GetExportDiagnostics() const {
  return m_exportDiagnostics;
}


// Returns false because truss GDTF loading is intentionally disabled in this stub.
bool LoadTrussGdtf(const std::string &, Truss &) { return false; }
// Returns false because truss-definition loading is intentionally disabled in this stub.
bool LoadTrussDefinition(const std::string &, Truss &) { return false; }
// Returns false because legacy conversion is intentionally disabled in this stub.
bool ConvertLegacyGtrussToGdtf(const std::filesystem::path &, const std::filesystem::path &, std::string *) { return false; }
// Returns false because GDTF building is intentionally disabled in this stub.
bool BuildTrussGdtfFromInstance(const Truss &, const std::filesystem::path &, std::string *) { return false; }
