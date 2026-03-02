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
#include "projectutils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
std::string DecodePathEscapes(const std::string &input) {
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (i + 2 < input.size() && input[i] == '%' && input[i + 1] == '2' &&
        input[i + 2] == '0') {
      out.push_back(' ');
      i += 2;
      continue;
    }
    out.push_back(input[i]);
  }
  return out;
}

bool EqualIgnoreCaseAscii(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    const unsigned char a = static_cast<unsigned char>(lhs[i]);
    const unsigned char b = static_cast<unsigned char>(rhs[i]);
    if (std::tolower(a) != std::tolower(b))
      return false;
  }
  return true;
}

bool MatchesFileNameWithExtensionTolerance(const fs::path &candidate,
                                           const std::string &fileName) {
  if (candidate.filename().string() == fileName)
    return true;
  const fs::path target(fileName);
  if (candidate.stem().string() != target.stem().string())
    return false;
  return EqualIgnoreCaseAscii(candidate.extension().string(),
                              target.extension().string());
}

std::string FindFileRecursive(const std::string &baseDir,
                              const std::string &fileName) {
  if (baseDir.empty())
    return {};
  std::error_code ec;
  fs::recursive_directory_iterator it(
      baseDir, fs::directory_options::skip_permission_denied, ec);
  fs::recursive_directory_iterator end;
  if (ec)
    return {};
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }
    if (MatchesFileNameWithExtensionTolerance(it->path(), fileName))
      return it->path().string();
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

std::string ResolveExistingPath(const fs::path &path) {
  if (path.empty())
    return {};
  std::error_code ec;
  if (fs::exists(path, ec))
    return path.string();

  const fs::path parent = path.parent_path();
  const fs::path filename = path.filename();
  if (parent.empty() || filename.empty())
    return {};

  for (const auto &entry : fs::directory_iterator(parent, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }
    if (MatchesFileNameWithExtensionTolerance(entry.path(), filename.string()))
      return entry.path().string();
  }
  return {};
}

std::string ResolveGdtfPath(const std::string &base,
                            const std::string &spec) {
  if (spec.empty())
    return {};
  const std::string cleanSpec = DecodePathEscapes(spec);
  const std::string norm = NormalizePath(cleanSpec);

  fs::path absolute = fs::path(norm);
  if (absolute.is_absolute()) {
    const std::string resolved = ResolveExistingPath(absolute);
    if (!resolved.empty())
      return resolved;
  }

  const std::string relative = ResolveExistingPath(fs::path(base) / absolute);
  if (!relative.empty())
    return relative;

  const std::string fileName = fs::path(norm).filename().string();
  const std::array<std::string, 1> librarySubdirs = {"fixtures"};
  for (const std::string &subdir : librarySubdirs) {
    const fs::path root = fs::u8path(ProjectUtils::GetDefaultLibraryPath(subdir));
    const std::string direct = ResolveExistingPath(root / fileName);
    if (!direct.empty())
      return direct;
    const std::string recursive = FindFileRecursive(root.string(), fileName);
    if (!recursive.empty())
      return recursive;
  }

  return FindFileRecursive(base, fileName);
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
  if (modelKey.empty())
    modelKey = "unknown";
  return modelKey;
}
