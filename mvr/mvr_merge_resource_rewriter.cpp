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
#include "mvr_merge_resource_rewriter.h"

#include "file_import_utils.h"

#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace mvr {
namespace {
namespace fs = std::filesystem;

constexpr const char *kMergedResourceRoot = "resources/mvr_merge";

// Trims leading and trailing ASCII whitespace from a resource reference.
std::string TrimAscii(std::string value) {
  auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
    value.erase(value.begin());
  while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
    value.pop_back();
  return value;
}

// Returns true when a relative archive path cannot escape the scene base path.
bool IsSafeRelativePath(const fs::path &path) {
  if (path.empty() || path.is_absolute())
    return false;
  for (const auto &part : path) {
    if (part == "..")
      return false;
  }
  return true;
}

// Builds a filesystem path for a scene resource reference.
fs::path ResolveSceneResourcePath(const std::string &basePath,
                                  const std::string &resourcePath) {
  fs::path path = fs::path(resourcePath);
  if (path.is_absolute() || basePath.empty())
    return path;
  return fs::path(basePath) / path;
}

// Converts a path reference to the preferred project-relative destination path.
fs::path PreferredProjectRelativePath(const std::string &resourcePath) {
  const fs::path rawPath = fs::path(resourcePath);
  if (IsSafeRelativePath(rawPath))
    return rawPath.lexically_normal();

  const fs::path fileName = rawPath.filename().empty()
                                ? fs::path("resource.bin")
                                : rawPath.filename();
  return fs::path(kMergedResourceRoot) / fileName;
}

// Builds a hash-suffixed relative path for resource conflicts.
fs::path BuildConflictRelativePath(const fs::path &relativePath,
                                   const std::string &sourceSha256) {
  const fs::path parent = relativePath.parent_path();
  const std::string renamed =
      FileImportUtils::BuildStableRenamedFilename(relativePath, sourceSha256);
  return parent.empty() ? fs::path(renamed) : parent / fs::path(renamed);
}

// Compares two regular files by SHA-256 when both hashes can be computed.
bool FilesHaveSameContent(const fs::path &path,
                          const std::string &expectedSha256) {
  const auto sha256 = FileImportUtils::ComputeFileSha256(path);
  return sha256 && *sha256 == expectedSha256;
}

// Copies one imported resource and returns its target-relative reference.
std::optional<std::string>
CopyAndRewriteResource(const MvrScene &target, const MvrScene &imported,
                       const std::string &resourcePath) {
  const std::string trimmed = TrimAscii(resourcePath);
  if (trimmed.empty() || target.basePath.empty())
    return std::nullopt;

  const fs::path sourcePath =
      ResolveSceneResourcePath(imported.basePath, trimmed);
  std::error_code ec;
  if (!fs::is_regular_file(sourcePath, ec) || ec)
    return std::nullopt;

  const auto sourceSha256 = FileImportUtils::ComputeFileSha256(sourcePath);
  if (!sourceSha256)
    return std::nullopt;

  fs::path relativePath = PreferredProjectRelativePath(trimmed);
  fs::path targetPath = fs::path(target.basePath) / relativePath;
  if (fs::exists(targetPath, ec) && !ec &&
      !FilesHaveSameContent(targetPath, *sourceSha256)) {
    relativePath = BuildConflictRelativePath(relativePath, *sourceSha256);
    targetPath = fs::path(target.basePath) / relativePath;
    int suffix = 2;
    while (fs::exists(targetPath, ec) && !ec &&
           !FilesHaveSameContent(targetPath, *sourceSha256)) {
      const fs::path parent = relativePath.parent_path();
      const fs::path stem = relativePath.stem();
      const fs::path ext = relativePath.extension();
      const fs::path candidate =
          parent / fs::path(stem.string() + "_" + std::to_string(suffix++) +
                            ext.string());
      relativePath = candidate;
      targetPath = fs::path(target.basePath) / relativePath;
    }
  }

  if (fs::exists(targetPath, ec) && !ec &&
      FilesHaveSameContent(targetPath, *sourceSha256))
    return relativePath.generic_string();

  fs::create_directories(targetPath.parent_path(), ec);
  if (ec)
    return std::nullopt;
  fs::copy_file(sourcePath, targetPath, fs::copy_options::overwrite_existing,
                ec);
  if (ec)
    return std::nullopt;
  return relativePath.generic_string();
}

// Rewrites one mutable resource reference after registration.
void RewriteResourceReference(
    const MvrScene &target, const MvrScene &imported, std::string &resourcePath,
    std::unordered_map<std::string, std::string> &cache) {
  const std::string trimmed = TrimAscii(resourcePath);
  if (trimmed.empty())
    return;

  const auto cacheIt = cache.find(trimmed);
  if (cacheIt != cache.end()) {
    resourcePath = cacheIt->second;
    return;
  }

  const auto rewritten = CopyAndRewriteResource(target, imported, trimmed);
  if (!rewritten)
    return;
  cache.emplace(trimmed, *rewritten);
  resourcePath = *rewritten;
}

} // namespace

// Copies imported MVR resources into the target scene and rewrites references.
void RewriteImportedSceneResourceReferences(const MvrScene &target,
                                            MvrScene &imported) {
  std::unordered_map<std::string, std::string> rewrittenByOriginal;

  for (auto &[uuid, fixture] : imported.fixtures)
    RewriteResourceReference(target, imported, fixture.gdtfSpec,
                             rewrittenByOriginal);

  for (auto &[uuid, truss] : imported.trusses) {
    RewriteResourceReference(target, imported, truss.gdtfSpec,
                             rewrittenByOriginal);
    RewriteResourceReference(target, imported, truss.symbolFile,
                             rewrittenByOriginal);
    RewriteResourceReference(target, imported, truss.modelFile,
                             rewrittenByOriginal);
    RewriteResourceReference(target, imported,
                             truss.perastageAuxGdtfArchivePath,
                             rewrittenByOriginal);
  }

  for (auto &[uuid, support] : imported.supports) {
    RewriteResourceReference(target, imported, support.gdtfSpec,
                             rewrittenByOriginal);
    RewriteResourceReference(target, imported, support.modelFile,
                             rewrittenByOriginal);
    for (auto &geometry : support.geometries)
      RewriteResourceReference(target, imported, geometry.modelFile,
                               rewrittenByOriginal);
  }

  for (auto &[uuid, object] : imported.sceneObjects) {
    RewriteResourceReference(target, imported, object.modelFile,
                             rewrittenByOriginal);
    for (auto &geometry : object.geometries)
      RewriteResourceReference(target, imported, geometry.modelFile,
                               rewrittenByOriginal);
  }

  for (auto &[uuid, file] : imported.symdefFiles)
    RewriteResourceReference(target, imported, file, rewrittenByOriginal);

  for (auto &[uuid, geometries] : imported.symdefGeometries) {
    for (auto &geometry : geometries)
      RewriteResourceReference(target, imported, geometry.file,
                               rewrittenByOriginal);
  }
}

} // namespace mvr
