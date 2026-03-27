#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class DictionaryImportPolicy {
  AddMissing,
  AddAndOverwrite,
  ReplaceAll
};

struct DictionaryImportSummary {
  size_t added_count = 0;
  size_t overwritten_count = 0;
  size_t skipped_count = 0;
  size_t missing_files_count = 0;
  std::vector<std::string> missing_file_examples;
  std::vector<std::string> errors;

  [[nodiscard]] bool HasErrors() const { return !errors.empty(); }
};
