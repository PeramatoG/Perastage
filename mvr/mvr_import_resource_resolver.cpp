/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "mvr_import_resource_resolver.h"

#include "filesystem_path_utils.h"
#include "gdtfloader.h"
#include "mvr_import_package.h"
#include "primitive_model_resources.h"
#include "trussloader.h"
#include "uuidutils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>

namespace fs = std::filesystem;

namespace mvr {
namespace {

// Trims surrounding ASCII whitespace from imported identifiers.
std::string Trim(const std::string &text) {
  const size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// Converts ASCII characters to lower case for compatibility comparisons.
std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

// Extracts a normalized digit signature from a mode name.
std::string DigitSignature(const std::string &text) {
  std::string digits;
  for (unsigned char c : text) {
    if (std::isdigit(c))
      digits.push_back(static_cast<char>(c));
  }
  const size_t first = digits.find_first_not_of('0');
  if (first == std::string::npos)
    return digits.empty() ? std::string{} : "0";
  return digits.substr(first);
}

// Normalizes a GDTF filename for permissive archive lookup.
std::string NormalizeGdtfLookupKey(const std::string &value) {
  const std::string normalized = NormalizeImportArchivePath(value);
  if (normalized.empty())
    return {};
  const fs::path path = PathUtils::PathFromUtf8(normalized);
  std::string extension = ToLowerAscii(path.extension().string());
  if (extension.empty())
    extension = ".gdtf";
  return ToLowerAscii(Trim(path.stem().string()) + extension);
}

// Normalizes fixture names used by canonical Perastage filenames.
std::string NormalizeFixtureName(std::string value) {
  value = ToLowerAscii(Trim(value));
  value.erase(std::remove_if(value.begin(), value.end(),
                             [](unsigned char c) {
                               return std::isspace(c) != 0 || c == '_' ||
                                      c == '-';
                             }),
              value.end());
  return value;
}

// Extracts the fixture segment from a canonical Perastage GDTF filename.
std::string PerastageFixtureName(const fs::path &path) {
  const std::string stem = path.stem().string();
  const size_t first = stem.find('@');
  const size_t second = first == std::string::npos ? std::string::npos
                                                   : stem.find('@', first + 1);
  if (second == std::string::npos ||
      NormalizeFixtureName(stem.substr(second + 1)) != "perastage")
    return {};
  return stem.substr(first + 1, second - first - 1);
}

// Resolves a GDTF spec with the existing permissive filename fallbacks.
std::string FindGdtfPath(const fs::path &basePath, const std::string &spec) {
  const std::string normalized = NormalizeImportArchivePath(spec);
  if (normalized.empty())
    return {};
  const fs::path relative = PathUtils::PathFromUtf8(normalized);
  fs::path candidate = basePath.empty() ? relative : basePath / relative;
  std::error_code ec;
  if (ToLowerAscii(candidate.extension().string()) == ".gdtf" &&
      fs::exists(candidate, ec) && !ec)
    return PathUtils::PathToUtf8(candidate);
  ec.clear();
  if (!candidate.has_extension()) {
    fs::path withExtension = candidate;
    withExtension += ".gdtf";
    if (fs::exists(withExtension, ec) && !ec)
      return PathUtils::PathToUtf8(withExtension);
    ec.clear();
  }
#if defined(_WIN32)
  if (basePath.empty())
    return {};
  const fs::path lookupDir = basePath;
#else
  const fs::path lookupDir = basePath.empty() ? fs::current_path(ec) : basePath;
#endif
  if (ec || !fs::exists(lookupDir, ec) || ec)
    return {};
  const std::string expectedStem =
      ToLowerAscii(Trim(relative.filename().stem().string()));
  const std::string expectedFixture = NormalizeFixtureName(expectedStem);
  const std::string expectedKey = NormalizeGdtfLookupKey(normalized);
  for (const auto &entry : fs::directory_iterator(lookupDir, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file() ||
        ToLowerAscii(entry.path().extension().string()) != ".gdtf")
      continue;
    const fs::path entryPath = entry.path();
    const std::string perastageFixtureName =
        PerastageFixtureName(entryPath.filename());
    if (ToLowerAscii(Trim(entryPath.stem().string())) == expectedStem ||
        NormalizeGdtfLookupKey(entryPath.filename().generic_string()) ==
            expectedKey ||
        (!perastageFixtureName.empty() &&
         NormalizeFixtureName(perastageFixtureName) == expectedFixture))
      return PathUtils::PathToUtf8(entryPath);
  }
  return {};
}

// Compares path components with native platform case semantics.
bool SamePathComponent(const fs::path &lhs, const fs::path &rhs) {
#if defined(_WIN32)
  return ToLowerAscii(lhs.string()) == ToLowerAscii(rhs.string());
#else
  return lhs == rhs;
#endif
}

// Reports whether a normalized path stays within a normalized base path.
bool IsWithin(const fs::path &candidate, const fs::path &base) {
  auto candidateIt = candidate.begin();
  for (auto baseIt = base.begin(); baseIt != base.end();
       ++baseIt, ++candidateIt) {
    if (candidateIt == candidate.end() ||
        !SamePathComponent(*candidateIt, *baseIt))
      return false;
  }
  return true;
}

// Produces an absolute normalized path without throwing.
fs::path NormalizedAbsolute(const fs::path &path, std::error_code &ec) {
  fs::path absolute = path;
  if (!absolute.is_absolute()) {
    absolute = fs::absolute(path, ec);
    if (ec)
      return {};
  }
  std::error_code canonicalError;
  const fs::path canonical = fs::weakly_canonical(absolute, canonicalError);
  return canonicalError ? absolute.lexically_normal()
                        : canonical.lexically_normal();
}

} // namespace

// Creates a resolver scoped to one extracted scene workspace.
MvrImportResourceResolver::MvrImportResourceResolver(
    fs::path sceneBasePath, ArchivePathRemapper archivePathRemapper)
    : sceneBasePath_(std::move(sceneBasePath)),
      archivePathRemapper_(std::move(archivePathRemapper)) {}

// Applies package-provided archive entry remapping when available.
std::string
MvrImportResourceResolver::RemapArchivePath(const std::string &path) const {
  return archivePathRemapper_ ? archivePathRemapper_(path) : path;
}

// Resolves an imported scene reference at the native filesystem boundary.
fs::path
MvrImportResourceResolver::ResolveScenePath(const std::string &path) const {
  const fs::path native = PathUtils::PathFromUtf8(path);
  return native.is_absolute() || sceneBasePath_.empty()
             ? native
             : sceneBasePath_ / native;
}

// Makes a path scene-relative only when it remains inside the scene workspace.
std::string
MvrImportResourceResolver::MakeSceneRelative(const fs::path &path) const {
  if (path.empty())
    return {};
  if (sceneBasePath_.empty() || !path.is_absolute())
    return PathUtils::PathToUtf8(path);
  std::error_code ec;
  const fs::path base = NormalizedAbsolute(sceneBasePath_, ec);
  if (ec)
    return PathUtils::PathToUtf8(path);
  const fs::path candidate = NormalizedAbsolute(path, ec);
  if (ec || !IsWithin(candidate, base))
    return PathUtils::PathToUtf8(path);
  const fs::path relative = fs::relative(candidate, base, ec);
  return ec || relative.empty() ? PathUtils::PathToUtf8(path)
                                : PathUtils::PathToUtf8(relative);
}

// Normalizes and resolves a fixture GDTF spec for scene storage.
std::string
MvrImportResourceResolver::NormalizeGdtfSpec(const std::string &spec) {
  const std::string normalized = NormalizeImportArchivePath(spec);
  return normalized.empty() ? std::string{}
                            : MakeSceneRelative(PathUtils::PathFromUtf8(
                                  ResolveGdtfPath(normalized)));
}

// Normalizes a support GDTF spec while preserving missing resources.
std::string MvrImportResourceResolver::NormalizeSupportGdtfSpec(
    const std::string &spec) const {
  const std::string normalized = NormalizeImportArchivePath(spec);
  if (normalized.empty())
    return {};
  const std::string resolved = FindGdtfPath(sceneBasePath_, normalized);
  return MakeSceneRelative(
      PathUtils::PathFromUtf8(resolved.empty() ? spec : resolved));
}

// Resolves and caches a GDTF spec for the lifetime of this resolver.
const std::string &
MvrImportResourceResolver::ResolveGdtfPath(const std::string &spec) {
  const std::string normalized = NormalizeImportArchivePath(spec);
  if (normalized.empty())
    return emptyPath_;
  const auto found = resolvedGdtfPaths_.find(normalized);
  if (found != resolvedGdtfPaths_.end())
    return found->second;
  std::string resolved = FindGdtfPath(sceneBasePath_, normalized);
  if (resolved.empty())
    resolved = PathUtils::PathToUtf8(ResolveScenePath(normalized));
  return resolvedGdtfPaths_.emplace(normalized, std::move(resolved))
      .first->second;
}

// Reports whether a resolved GDTF path names a regular file.
bool MvrImportResourceResolver::GdtfFileExists(const std::string &path) const {
  std::error_code ec;
  return !path.empty() &&
         fs::is_regular_file(PathUtils::PathFromUtf8(path), ec) && !ec;
}

// Builds a stable native-filesystem identity key for a resolved GDTF.
std::string
MvrImportResourceResolver::GdtfIdentityKey(const std::string &path) const {
  return path.empty() ? std::string{}
                      : PathUtils::BuildFilesystemIdentityKey(
                            PathUtils::PathFromUtf8(path));
}

// Loads and caches the mode list for a GDTF resource.
const std::vector<std::string> &
MvrImportResourceResolver::GdtfModes(const std::string &path) {
  const std::string key = GdtfIdentityKey(path);
  const auto found = gdtfModes_.find(key);
  if (found != gdtfModes_.end())
    return found->second;
  return gdtfModes_
      .emplace(key, GdtfFileExists(path) ? GetGdtfModes(path)
                                         : std::vector<std::string>{})
      .first->second;
}

// Loads and caches one GDTF mode channel count.
int MvrImportResourceResolver::GdtfModeChannelCount(const std::string &path,
                                                    const std::string &mode) {
  if (!GdtfFileExists(path))
    return -1;
  auto &counts = gdtfModeChannelCounts_[GdtfIdentityKey(path)];
  const auto found = counts.find(mode);
  if (found != counts.end())
    return found->second;
  return counts.emplace(mode, GetGdtfModeChannelCount(path, mode))
      .first->second;
}

// Selects a compatible mode using the established importer fallback order.
std::string MvrImportResourceResolver::SelectMode(
    const std::vector<std::string> &modes, const std::string &requestedMode,
    std::optional<int> channelCountHint,
    const ModeChannelCountLookup &channelCount) {
  if (modes.empty())
    return requestedMode;
  const std::string requested = ToLowerAscii(Trim(requestedMode));
  if (!requested.empty()) {
    for (const std::string &mode : modes) {
      if (ToLowerAscii(Trim(mode)) == requested)
        return mode;
    }
  }
  const std::string digits = DigitSignature(requested);
  if (!digits.empty()) {
    for (const std::string &mode : modes) {
      if (DigitSignature(ToLowerAscii(Trim(mode))) == digits)
        return mode;
    }
  }
  if (channelCountHint && *channelCountHint > 0) {
    for (const std::string &mode : modes) {
      if (channelCount(mode) == *channelCountHint)
        return mode;
    }
  }
  for (const std::string &mode : modes) {
    const std::string normalized = ToLowerAscii(Trim(mode));
    if (normalized == "default" || normalized == "standard")
      return mode;
  }
  return modes.front();
}

// Resolves a requested mode using cached GDTF mode information.
std::string MvrImportResourceResolver::ResolveGdtfMode(
    const std::string &path, const std::string &requestedMode,
    std::optional<int> channelCountHint) {
  return SelectMode(GdtfModes(path), requestedMode, channelCountHint,
                    [&](const std::string &mode) {
                      return GdtfModeChannelCount(path, mode);
                    });
}

// Loads and caches fixture metadata exposed by a GDTF resource.
const ImportGdtfMetadata &
MvrImportResourceResolver::FixtureMetadata(const std::string &path) {
  if (!GdtfFileExists(path))
    return emptyMetadata_;
  const std::string key = GdtfIdentityKey(path);
  const auto found = fixtureMetadata_.find(key);
  if (found != fixtureMetadata_.end())
    return found->second;
  ImportGdtfMetadata metadata;
  metadata.fixtureName = Trim(GetGdtfFixtureName(path));
  metadata.manufacturer = Trim(GetGdtfFixtureManufacturer(path));
  const std::string rawId = Trim(GetGdtfFixtureTypeId(path));
  metadata.fixtureTypeId = CanonicalizeUuid(rawId);
  if (metadata.fixtureTypeId.empty())
    metadata.fixtureTypeId = rawId;
  metadata.hasProperties =
      GetGdtfProperties(path, metadata.weightKg, metadata.powerW);
  return fixtureMetadata_.emplace(key, std::move(metadata)).first->second;
}

// Loads and caches either the successful or failed truss definition result.
bool MvrImportResourceResolver::LoadTrussDefinition(const std::string &path,
                                                    Truss &out) {
  if (path.empty())
    return false;
  const std::string key = GdtfIdentityKey(path);
  auto found = trussDefinitions_.find(key);
  if (found == trussDefinitions_.end()) {
    Truss loaded;
    found = trussDefinitions_
                .emplace(key, ::LoadTrussDefinition(path, loaded)
                                  ? std::optional<Truss>(std::move(loaded))
                                  : std::nullopt)
                .first;
  }
  if (!found->second)
    return false;
  out = *found->second;
  return true;
}

// Loads and caches a dictionary entry for a fixture type.
const std::optional<GdtfDictionary::Entry> &
MvrImportResourceResolver::DictionaryEntry(const std::string &typeName) {
  static const std::optional<GdtfDictionary::Entry> empty;
  if (typeName.empty())
    return empty;
  const auto found = dictionaryEntries_.find(typeName);
  if (found != dictionaryEntries_.end())
    return found->second;
  return dictionaryEntries_.emplace(typeName, GdtfDictionary::Get(typeName))
      .first->second;
}

// Normalizes and resolves a scene geometry resource filename.
std::string MvrImportResourceResolver::NormalizeGeometryFile(
    const std::string &fileName) const {
  const std::string trimmed = Trim(fileName);
  if (trimmed.empty())
    return {};
  std::string normalized =
      PathUtils::PathToUtf8(PathUtils::PathFromUtf8(trimmed));
  std::string primitive;
  if (ResolvePrimitiveTokenFromModelRef(normalized, primitive))
    return primitive;
  normalized = RemapArchivePath(normalized);
  if (ResolvePrimitiveTokenFromModelRef(normalized, primitive))
    return primitive;
  fs::path resolved = ResolveScenePath(normalized);
  if (!PathUtils::PathFromUtf8(normalized).has_extension()) {
    const std::array<std::string, 3> extensions = {".gltf", ".glb", ".3ds"};
    for (const std::string &extension : extensions) {
      fs::path candidate = resolved;
      candidate += extension;
      std::error_code ec;
      if (fs::exists(candidate, ec) && !ec) {
        resolved = std::move(candidate);
        break;
      }
    }
    if (!resolved.has_extension())
      resolved += ".3ds";
  }
  return MakeSceneRelative(resolved);
}

} // namespace mvr
