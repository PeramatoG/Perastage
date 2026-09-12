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

#include "fixture.h"
#include "gdtf_fixture_category.h"
#include "mvr_import_types.h"
#include "truss.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tinyxml2 {
class XMLElement;
}

namespace mvr {

class MvrImportResourceResolver;

struct SceneReadLegacyFixtureIdentity {
  std::string stableId;
  std::string instanceName;
};

struct SceneReadFixtureTypeInfo {
  std::string category;
  std::string categorySource;
  std::string visualColorHex;
};

struct SceneReadFixtureIdentifiers {
  int fixtureId = 0;
  int fixtureIdNumeric = 0;
  int unitNumber = 0;
  std::string fixtureIdText;
};

struct SceneReadCachedCategory {
  std::string category;
  std::string source;
  std::string reason;
};

struct SceneReadGdtfConflict {
  std::string type;
  std::string requestedFixtureName;
  std::string mvrPath;
  std::string appPath;
  std::string manufacturer;
  std::string fixtureName;
  std::string fixtureTypeId;
  std::string modeName;
  int footprint = 0;
  bool hasDictionaryEntry = false;
};

struct MvrSceneReadServices {
  MvrImportResourceResolver &resources;
  std::function<std::string(tinyxml2::XMLElement *, const char *)> textOf;
  std::function<void(tinyxml2::XMLElement *, const char *, int &)> intOf;
  std::function<void(tinyxml2::XMLElement *, std::string &, int &)> fixtureIdOf;
  std::function<void(tinyxml2::XMLElement *, const char *, const std::string &,
                     Matrix &, bool)>
      parseMatrixOrIdentity;
  std::function<std::string(const std::string &, const std::string &,
                            const std::string &)>
      buildFixtureTypeInfoKey;
  std::function<std::string(const char *, tinyxml2::XMLElement *,
                            const std::string &, const Matrix &,
                            const std::string &)>
      resolveStableUuid;
  std::function<std::string(const char *, tinyxml2::XMLElement *,
                            const std::string &, const Matrix &)>
      referenceUuid;
  std::function<void(const std::string &, const std::string &)>
      recordFixtureUuid;
  std::function<std::string(const std::string &)> ensurePosition;
  std::function<void(tinyxml2::XMLElement *, std::vector<SymdefGeometry> &,
                     std::string &, Matrix &)>
      resolveSymdef;
  std::function<void(std::vector<GeometryInstance> &, const std::string &,
                     const Matrix &, const std::string &, const std::string &,
                     const std::string &)>
      appendGeometry;
  std::function<void(std::string, int, int)> reportProgress;
  std::function<void(const std::string &)> logDebug;
  std::function<void(const std::string &)> logInfo;
  std::function<void(const std::string &)> logWarning;
  std::function<void(const std::string &)> logError;
};

struct MvrSceneFixtureMetadata {
  const std::unordered_map<std::string, SceneReadFixtureTypeInfo> &types;
  const std::unordered_map<std::string, SceneReadFixtureIdentifiers>
      &identifiers;
  const std::unordered_map<std::string, std::string> &colors;
  const std::unordered_set<std::string> &colorMetadataUuids;
};

struct MvrSceneRiggingMetadata {
  const std::unordered_map<std::string, tinyxml2::XMLElement *> &trussInfo;
  const std::unordered_map<std::string, tinyxml2::XMLElement *> &hoistInfo;
  const std::unordered_map<std::string, std::string> &trussGdtfByType;
  const std::unordered_map<std::string, std::string> &trussTypeByInstance;
};

struct MvrSceneGeometryMetadata {
  const std::unordered_map<std::string, std::vector<std::string>>
      &primitiveModels;
  const std::unordered_map<std::string, std::string> &legacyPositions;
};

struct MvrSceneLayerMetadata {
  const std::unordered_map<std::string, std::string> &colorsByUuid;
  const std::unordered_map<std::string, std::string> &colorsByName;
};

struct MvrSceneReadMetadata {
  MvrSceneFixtureMetadata fixtures;
  MvrSceneRiggingMetadata rigging;
  MvrSceneGeometryMetadata geometry;
  MvrSceneLayerMetadata layers;
};

struct MvrSceneFixtureReadState {
  std::unordered_map<std::string, SceneReadGdtfConflict> &pendingGdtfConflicts;
  std::unordered_map<std::string, SceneReadCachedCategory> &categoriesByType;
  std::unordered_map<std::string, GdtfFixtureCategory::InferenceResult>
      &categoryInferences;
  std::unordered_set<std::string> &consumedColors;
};

struct MvrSceneRiggingReadState {
  std::unordered_set<std::string> &consumedTrussInfo;
  std::unordered_set<std::string> &consumedHoistInfo;
};

struct MvrSceneReadState {
  MvrSceneFixtureReadState fixtures;
  MvrSceneRiggingReadState rigging;
};

struct MvrSceneReadMetrics {
  int trussSymbolSymdefPreservedCount = 0;
  std::unordered_map<std::string, int> trussSymbolSymdefPreservedBySymdef;
  int preservedGroupObjectCount = 0;
};

// Reads supported logical scene nodes while preserving hierarchy and
// transforms.
void ReadMvrSceneNodes(tinyxml2::XMLElement *sceneNode, MvrScene &scene,
                       MvrImportResult &importResult,
                       const MvrImportOptions &options,
                       const MvrSceneReadServices &services,
                       const MvrSceneReadMetadata &metadata,
                       MvrSceneReadState &state, MvrSceneReadMetrics &metrics);

} // namespace mvr
