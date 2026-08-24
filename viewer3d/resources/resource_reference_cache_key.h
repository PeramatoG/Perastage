#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace viewer3d::resources {

// Builds a platform-neutral key for an unresolved scene resource reference.
inline std::string
BuildResourceReferenceCacheKey(const std::string &reference) {
  std::string normalized = reference;
  const auto isWhitespace = [](unsigned char value) {
    return std::isspace(value) != 0;
  };
  normalized.erase(normalized.begin(),
                   std::find_if_not(normalized.begin(), normalized.end(),
                                    isWhitespace));
  normalized.erase(
      std::find_if_not(normalized.rbegin(), normalized.rend(), isWhitespace)
          .base(),
      normalized.end());
  if (normalized.size() >= 2 && normalized.front() == '"' &&
      normalized.back() == '"') {
    normalized = normalized.substr(1, normalized.size() - 2);
  }
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  if (normalized.empty())
    return {};
  return std::filesystem::path(normalized).lexically_normal().generic_string();
}

} // namespace viewer3d::resources
