/* This file is part of Perastage. */
#include "mvr_scene_resource_lookup.h"
#include "filesystem_path_utils.h"
#include <algorithm>
#include <cctype>
namespace mvr {
std::filesystem::path
ResolveExistingSceneResource(const std::string &reference,
                             const std::filesystem::path &base,
                             bool allowFilenameAlias) {
  if (reference.empty())
    return {};
  auto path = PathUtils::PathFromUtf8(reference);
  if (path.is_relative())
    path = base / path;
  std::error_code error;
  if (std::filesystem::is_regular_file(path, error)) {
    auto canonical = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
  }
  if (!allowFilenameAlias || base.empty())
    return {};
  const auto fold = [](std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
  };
  const auto requested =
      fold(PathUtils::PathFromUtf8(reference).filename().generic_string());
  std::filesystem::path candidate;
  for (const auto &entry : std::filesystem::directory_iterator(base, error)) {
    if (error)
      break;
    std::error_code kindError;
    if (entry.is_regular_file(kindError) && !kindError &&
        fold(entry.path().filename().generic_string()) == requested) {
      if (!candidate.empty())
        return {}; // Ambiguous aliases are never guessed.
      candidate = entry.path();
    }
  }
  if (candidate.empty())
    return {};
  auto canonical = std::filesystem::weakly_canonical(candidate, error);
  return error ? candidate.lexically_normal() : canonical;
}
} // namespace mvr
