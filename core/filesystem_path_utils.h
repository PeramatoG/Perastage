#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace PathUtils {

std::filesystem::path PathFromUtf8(const std::string &text);
std::string PathToUtf8(const std::filesystem::path &path);
std::string BuildFilesystemIdentityKey(const std::filesystem::path &path);
std::string BuildFilesystemIdentityKey(const std::string &utf8Path);
std::string BuildFilesystemIdentityKey(const std::filesystem::path &path,
                                       const std::filesystem::path &basePath);
bool AreFilesystemPathsEquivalent(const std::filesystem::path &left,
                                  const std::filesystem::path &right,
                                  std::error_code &error);

} // namespace PathUtils
