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
#include "mvr_standard_field_projection.h"
#include "mvr_source_structure_service.h"
#include "mvr_model_resource_closure.h"
#include "mvr_model_dependencies.h"

#include "file_import_utils.h"
#include "filesystem_path_utils.h"

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
  fs::path path = PathUtils::PathFromUtf8(resourcePath);
  if (path.is_absolute() || basePath.empty())
    return path;
  return PathUtils::PathFromUtf8(basePath) / path;
}

// Converts a path reference to the preferred project-relative destination path.
fs::path PreferredProjectRelativePath(const std::string &resourcePath) {
  const fs::path rawPath = PathUtils::PathFromUtf8(resourcePath);
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

  auto extension = sourcePath.extension().string();
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if ((extension == ".3ds" || extension == ".gltf" || extension == ".glb") &&
      !InspectModelDependencies(sourcePath).references.empty()) {
    const auto closure = DiscoverModelResourceClosure(
        sourcePath, PathUtils::PathFromUtf8(imported.basePath));
    if (!closure.error.empty())
      return std::nullopt;
    const auto prefix =
        ModelResourcePrefix(closure, [&](const std::string &candidate) {
          for (const auto &file : closure.files) {
            const auto destination = PathUtils::PathFromUtf8(target.basePath) /
                                     candidate /
                                     file.lexically_relative(closure.root);
            if (fs::exists(destination, ec)) {
              const auto hash = FileImportUtils::ComputeFileSha256(file);
              if (!hash || !FilesHaveSameContent(destination, *hash))
                return false;
            }
          }
          return true;
        });
    if (prefix.empty())
      return std::nullopt;
    for (const auto &file : closure.files) {
      const auto destination = PathUtils::PathFromUtf8(target.basePath) /
                               prefix / file.lexically_relative(closure.root);
      fs::create_directories(destination.parent_path(), ec);
      if (ec)
        return std::nullopt;
      if (!fs::exists(destination, ec)) {
        fs::copy_file(file, destination, fs::copy_options::none, ec);
        if (ec)
          return std::nullopt;
      }
    }
    const auto primary = fs::weakly_canonical(sourcePath, ec);
    if (ec)
      return std::nullopt;
    return prefix + ModelResourceRelativeName(primary, closure.root);
  }
  fs::path relativePath = PreferredProjectRelativePath(trimmed);
  fs::path targetPath = PathUtils::PathFromUtf8(target.basePath) / relativePath;
  if (fs::exists(targetPath, ec) && !ec &&
      !FilesHaveSameContent(targetPath, *sourceSha256)) {
    relativePath = BuildConflictRelativePath(relativePath, *sourceSha256);
    targetPath = PathUtils::PathFromUtf8(target.basePath) / relativePath;
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
      targetPath = PathUtils::PathFromUtf8(target.basePath) / relativePath;
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
bool RewriteResourceReference(
    const MvrScene &target, const MvrScene &imported, std::string &resourcePath,
    std::unordered_map<std::string, std::string> &cache) {
  const std::string trimmed = TrimAscii(resourcePath);
  if (trimmed.empty())
    return false;

  const auto cacheIt = cache.find(trimmed);
  if (cacheIt != cache.end()) {
    resourcePath = cacheIt->second;
    return true;
  }

  const auto rewritten = CopyAndRewriteResource(target, imported, trimmed);
  if (!rewritten)
    return false;
  cache.emplace(trimmed, *rewritten);
  resourcePath = *rewritten;
  return true;
}

} // namespace

// Copies imported MVR resources into the target scene and rewrites references.
void RewriteImportedSceneResourceReferences(const MvrScene &target,
                                            MvrScene &imported) {
  std::unordered_map<std::string, std::string> rewrittenByOriginal;
  ReconcileSourceStructure(imported, {}, [&](const std::string &source) {
    std::string path = source;
    return RewriteResourceReference(target, imported, path, rewrittenByOriginal)
               ? path
               : std::string();
  });
  ReconcileStandardSourceFields(imported, {}, [&](const std::string &source) {
    std::string path = source;
    return RewriteResourceReference(target, imported, path, rewrittenByOriginal) ? path : std::string();
  });

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

  for (auto &[uuid, definition] : imported.symdefs) {
    for (auto &geometry : definition.geometries) {
      if (!RewriteResourceReference(target, imported, geometry.file, rewrittenByOriginal))
        imported.preservationIssues.push_back({"Geometry3D", uuid,
            "Scene/AUXData/Symdef/ChildList/Geometry3D/@fileName", geometry.file,
            "Canonical Symdef resource could not be acquired during merge: " + geometry.file,
            "Resolve the resource failure and retry merging the intact source.",
            MvrPreservationIssueKind::ResourceFailure});
    }
  }

}

} // namespace mvr
