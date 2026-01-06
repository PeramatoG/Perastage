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
#include "legendutils.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
std::string FindFileRecursive(const std::string &baseDir,
                              const std::string &fileName) {
  if (baseDir.empty())
    return {};
  for (auto &p : fs::recursive_directory_iterator(baseDir)) {
    if (!p.is_regular_file())
      continue;
    if (p.path().filename() == fileName)
      return p.path().string();
  }
  return {};
}

std::string NormalizePath(const std::string &path) {
  std::string out = path;
  char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  return out;
}

std::string NormalizeModelKey(const std::string &path) {
  if (path.empty())
    return {};
  fs::path p(path);
  p = p.lexically_normal();
  return NormalizePath(p.string());
}

std::string ResolveGdtfPath(const std::string &base,
                            const std::string &spec) {
  if (spec.empty())
    return {};
  std::string norm = NormalizePath(spec);
  fs::path p = base.empty() ? fs::path(norm) : fs::path(base) / norm;
  if (fs::exists(p))
    return p.string();
  return FindFileRecursive(base, fs::path(norm).filename().string());
}
} // namespace

std::string BuildFixtureSymbolKey(const Fixture &fixture,
                                  const std::string &basePath) {
  std::string gdtfPath = ResolveGdtfPath(basePath, fixture.gdtfSpec);
  std::string modelKey = NormalizeModelKey(gdtfPath);
  if (modelKey.empty() && !fixture.gdtfSpec.empty())
    modelKey = NormalizeModelKey(fixture.gdtfSpec);
  if (modelKey.empty() && !fixture.typeName.empty())
    modelKey = fixture.typeName;
  return modelKey;
}
