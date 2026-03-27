#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace FileImportUtils {

enum class ConflictPolicy {
  Rename,
  Overwrite,
  Cancel,
};

struct CopyResult {
  bool success = false;
  bool copied = false;
  bool reusedExisting = false;
  std::filesystem::path finalPath;
  std::string sourceSha256;
  std::string finalSha256;
};

std::optional<std::string> ComputeFileSha256(const std::filesystem::path &path);
std::string BuildStableRenamedFilename(const std::filesystem::path &path,
                                       const std::string &sha256,
                                       size_t shortLen = 8);
std::string NowUtcIso8601();

CopyResult CopyWithConflictPolicy(const std::filesystem::path &sourcePath,
                                  const std::filesystem::path &targetPath,
                                  ConflictPolicy policy);

} // namespace FileImportUtils
