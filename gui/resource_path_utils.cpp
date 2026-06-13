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
#include "resource_path_utils.h"
#include "filesystem_path_utils.h"

#include "logger.h"

#include <filesystem>
#include <system_error>

namespace gui {
namespace {

namespace fs = std::filesystem;

// Builds a normalized absolute path using non-throwing filesystem operations.
fs::path NormalizedAbsolutePath(const fs::path &path, std::error_code &ec) {
  ec.clear();
  fs::path absolute = path;
  if (!absolute.is_absolute()) {
    absolute = fs::absolute(path, ec);
    if (ec)
      return {};
  }

  std::error_code canonicalEc;
  fs::path canonical = fs::weakly_canonical(absolute, canonicalEc);
  if (!canonicalEc)
    return canonical.lexically_normal();

  return absolute.lexically_normal();
}

// Returns true when candidate is inside base using normalized path components.
bool IsPathWithinBaseDirectory(const fs::path &candidate, const fs::path &base) {
  auto candidateIt = candidate.begin();
  auto baseIt = base.begin();
  for (; baseIt != base.end(); ++baseIt, ++candidateIt) {
    if (candidateIt == candidate.end() || *candidateIt != *baseIt)
      return false;
  }
  return true;
}

// Logs a warning when scene-relative path conversion cannot be completed safely.
void LogSceneRelativeConversionWarning(const std::string &logContext,
                                       const std::string &resourcePath,
                                       const std::string &basePath,
                                       const std::error_code &ec) {
  Logger::Instance().Log(
      Logger::Level::Warn,
      logContext + ": keeping absolute resource path '" + resourcePath +
          "' because it could not be made relative to scene base path '" +
          basePath + "': " + ec.message() + ".");
}

} // namespace

// Converts a resource path to a scene-relative path when it is safely under the scene base path.
std::string MakeSceneRelativeResourcePathOrOriginal(
    const std::string &basePath, const std::string &resourcePath,
    const std::string &logContext) {
  if (basePath.empty() || resourcePath.empty())
    return resourcePath;

  std::error_code ec;
  const fs::path resource = NormalizedAbsolutePath(PathUtils::PathFromUtf8(resourcePath), ec);
  if (ec) {
    LogSceneRelativeConversionWarning(logContext, resourcePath, basePath, ec);
    return resourcePath;
  }

  const fs::path base = NormalizedAbsolutePath(PathUtils::PathFromUtf8(basePath), ec);
  if (ec) {
    LogSceneRelativeConversionWarning(logContext, resourcePath, basePath, ec);
    return resourcePath;
  }

  if (!IsPathWithinBaseDirectory(resource, base))
    return resourcePath;

  fs::path relative = fs::relative(resource, base, ec);
  if (ec) {
    LogSceneRelativeConversionWarning(logContext, resourcePath, basePath, ec);
    return resourcePath;
  }

  return relative.lexically_normal().string();
}

} // namespace gui
