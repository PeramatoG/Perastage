#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace PathUtils {

std::filesystem::path PathFromUtf8(const std::string &text);
std::string PathToUtf8(const std::filesystem::path &path);
// Scene references use '/' and accept legacy Windows separators. Unsafe relative
// paths and symlinks escaping the scene root resolve to an empty path. Absolute
// native paths remain supported for explicitly selected external resources.
std::filesystem::path ResolveSceneResourcePath(const std::string &reference,
                                              const std::filesystem::path &sceneRoot);
std::string PortablePathToUtf8(const std::filesystem::path &path);
std::string BuildFilesystemIdentityKey(const std::filesystem::path &path);
std::string BuildFilesystemIdentityKey(const std::string &utf8Path);
std::string BuildFilesystemIdentityKey(const std::filesystem::path &path,
                                       const std::filesystem::path &basePath);
bool AreFilesystemPathsEquivalent(const std::filesystem::path &left,
                                  const std::filesystem::path &right,
                                  std::error_code &error);

} // namespace PathUtils
