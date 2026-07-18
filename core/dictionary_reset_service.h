#pragma once

#include "active_dictionary_storage.h"
#include "dictionary_import.h"

#include <filesystem>

namespace DictionaryResetService {

struct Request {
  ActiveDictionaryStorage::DictionaryKind kind;
  std::filesystem::path activeJsonPath;
  std::filesystem::path managedDefaultJsonPath;
  std::filesystem::path applicationBaseJsonPath;
};

DictionaryImportSummary ResetToDefaults(const Request &request);

} // namespace DictionaryResetService
