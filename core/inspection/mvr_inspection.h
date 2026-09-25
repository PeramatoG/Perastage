#pragma once

#include "inspection/package_inspection.h"
#include "inspection/xml_schema_validation.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace perastage::inspection {

// Records one stable scene-node total for summary and tree consumers.
struct MvrNodeCount {
  std::string type;
  std::size_t count = 0;

  // Compares deterministic node-count records by value.
  bool operator==(const MvrNodeCount &) const = default;
};

// Owns a path reference found in the parsed scene without extracted paths.
struct MvrResourceReference {
  std::string kind;
  std::string archivePath;

  // Compares deterministic resource-reference records by value.
  bool operator==(const MvrResourceReference &) const = default;
};

// Describes one immutable scene node and its authored hierarchy references.
struct MvrSceneNodeDescriptor {
  std::string kind;
  std::string uuid;
  std::string name;
  std::string layerUuid;
  std::string layerName;
  std::string parentGroupUuid;
  std::string resourceReference;
  std::vector<std::string> childUuids;

  // Compares deterministic scene descriptors by value.
  bool operator==(const MvrSceneNodeDescriptor &) const = default;
};

// Describes one retained Symdef and its packaged geometry references.
struct MvrSymdefDescriptor {
  std::string uuid;
  std::vector<std::string> resourceReferences;

  // Compares deterministic Symdef descriptors by value.
  bool operator==(const MvrSymdefDescriptor &) const = default;
};

// Preserves one foreign provider block without interpreting its schema.
struct MvrForeignUserDataDescriptor {
  std::string provider;
  std::string version;
  std::string xml;

  // Compares preserved foreign data by exact retained values.
  bool operator==(const MvrForeignUserDataDescriptor &) const = default;
};

// Carries the immutable, presentation-neutral facts read from one MVR.
struct MvrInspectionSnapshot {
  int versionMajor = 0;
  int versionMinor = 0;
  std::string provider;
  std::string providerVersion;
  std::string sceneDescriptionEntry;
  std::string sceneDescriptionXml;
  std::vector<std::string> embeddedGdtfEntries;
  std::vector<MvrResourceReference> referencedResources;
  std::vector<MvrSceneNodeDescriptor> layers;
  std::vector<MvrSceneNodeDescriptor> fixtures;
  std::vector<MvrSceneNodeDescriptor> trusses;
  std::vector<MvrSceneNodeDescriptor> supports;
  std::vector<MvrSceneNodeDescriptor> sceneObjects;
  std::vector<MvrSceneNodeDescriptor> groupObjects;
  std::vector<MvrSceneNodeDescriptor> positions;
  std::vector<MvrSymdefDescriptor> symdefs;
  std::vector<MvrForeignUserDataDescriptor> foreignUserData;
  std::vector<MvrNodeCount> nodeCounts;
};

// Combines INS-100 inventory with the established MVR import read model.
struct MvrInspectionResult {
  Result inspection;
  std::optional<PackageInventory> packageInventory;
  std::optional<MvrInspectionSnapshot> snapshot;
  std::vector<ValidationResult> validation;

  bool Success() const;
};

MvrInspectionResult InspectMvr(const Request &request);
MvrInspectionResult InspectMvr(const std::filesystem::path &sourcePath);
MvrInspectionResult InspectMvrBytes(const std::vector<std::uint8_t> &bytes,
                                    const Request &request = {});

} // namespace perastage::inspection
