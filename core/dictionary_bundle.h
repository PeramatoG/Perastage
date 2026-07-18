#pragma once

#include "dictionary_import.h"
#include "gdtfdictionary.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace DictionaryBundle {

enum class Type {
  Fixtures,
  Trusses,
};

struct PreparedImport {
  bool is_bundle = false;
  std::filesystem::path rewritten_snapshot_path;
  std::filesystem::path staging_directory;
  Type type = Type::Fixtures;
  std::vector<std::string> errors;
};

struct ApplyResult {
  DictionaryImportSummary summary;
  std::filesystem::path staged_snapshot_path;
};

bool ExportFixturesBundle(
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dict,
    const std::string &outputZipPath, std::string &error);
bool ExportTrussesBundle(
    const std::unordered_map<std::string, std::string> &dict,
    const std::string &outputZipPath, std::string &error);

PreparedImport PrepareBundleImport(const std::string &importPath, Type expectedType);
ApplyResult ApplyPreparedBundleImport(const PreparedImport &preparedImport,
                                      DictionaryImportPolicy policy);
bool ValidateBundleFile(const std::string &bundlePath, Type expectedType,
                        std::string &error);
void CleanupPreparedImport(const PreparedImport &preparedImport);

} // namespace DictionaryBundle
