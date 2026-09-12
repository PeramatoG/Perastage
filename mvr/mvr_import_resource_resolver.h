/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include "gdtfdictionary.h"
#include "truss.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mvr {

struct ImportGdtfMetadata {
  std::string fixtureName;
  std::string manufacturer;
  std::string fixtureTypeId;
  float weightKg = 0.0f;
  float powerW = 0.0f;
  bool hasProperties = false;
};

class MvrImportResourceResolver {
public:
  using ArchivePathRemapper = std::function<std::string(const std::string &)>;
  using ModeChannelCountLookup = std::function<int(const std::string &)>;

  explicit MvrImportResourceResolver(
      std::filesystem::path sceneBasePath,
      ArchivePathRemapper archivePathRemapper = {});

  std::string RemapArchivePath(const std::string &path) const;
  std::filesystem::path ResolveScenePath(const std::string &path) const;
  std::string MakeSceneRelative(const std::filesystem::path &path) const;
  std::string NormalizeGdtfSpec(const std::string &spec);
  std::string NormalizeSupportGdtfSpec(const std::string &spec) const;
  const std::string &ResolveGdtfPath(const std::string &spec);
  bool GdtfFileExists(const std::string &path) const;
  std::string GdtfIdentityKey(const std::string &path) const;
  const std::vector<std::string> &GdtfModes(const std::string &path);
  int GdtfModeChannelCount(const std::string &path, const std::string &mode);
  std::string ResolveGdtfMode(const std::string &path,
                              const std::string &requestedMode,
                              std::optional<int> channelCountHint);
  const ImportGdtfMetadata &FixtureMetadata(const std::string &path);
  bool LoadTrussDefinition(const std::string &path, Truss &out);
  const std::optional<GdtfDictionary::Entry> &
  DictionaryEntry(const std::string &typeName);
  std::string NormalizeGeometryFile(const std::string &fileName) const;

  static std::string SelectMode(const std::vector<std::string> &modes,
                                const std::string &requestedMode,
                                std::optional<int> channelCountHint,
                                const ModeChannelCountLookup &channelCount);

private:
  std::filesystem::path sceneBasePath_;
  ArchivePathRemapper archivePathRemapper_;
  std::unordered_map<std::string, std::string> resolvedGdtfPaths_;
  std::unordered_map<std::string, std::vector<std::string>> gdtfModes_;
  std::unordered_map<std::string, std::unordered_map<std::string, int>>
      gdtfModeChannelCounts_;
  std::unordered_map<std::string, ImportGdtfMetadata> fixtureMetadata_;
  std::unordered_map<std::string, std::optional<Truss>> trussDefinitions_;
  std::unordered_map<std::string, std::optional<GdtfDictionary::Entry>>
      dictionaryEntries_;
  const std::string emptyPath_;
  const ImportGdtfMetadata emptyMetadata_;
};

} // namespace mvr
