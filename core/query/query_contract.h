#pragma once

#include "scene_object_identity.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace perastage::query {

inline constexpr const char *kSummaryQueryId = "scene.summary";
inline constexpr const char *kSelectionQueryId = "scene.selection.get";
inline constexpr const char *kObjectsQueryId = "scene.objects.list";
inline constexpr const char *kObjectQueryId = "scene.object.get";
inline constexpr const char *kLayersQueryId = "scene.layers.list";
inline constexpr const char *kGroupsQueryId = "scene.groups.list";
inline constexpr const char *kPatchQueryId = "scene.patch.status";

struct Diagnostic {
  std::string code;
  std::string message;
  std::optional<scene_identity::ObjectReference> object;
  bool operator==(const Diagnostic &) const = default;
};

struct Summary {
  int mvrVersionMajor = 0;
  int mvrVersionMinor = 0;
  std::size_t fixtures = 0;
  std::size_t trusses = 0;
  std::size_t supports = 0;
  std::size_t sceneObjects = 0;
  std::size_t groups = 0;
  std::size_t layers = 0;
  bool operator==(const Summary &) const = default;
};

struct ObjectDescriptor {
  scene_identity::ObjectKind kind = scene_identity::ObjectKind::Fixture;
  std::string uuid;
  std::string name;
  std::string layerUuid;
  std::string parentGroupUuid;
  std::string typeName;
  std::string resource;
  // Stored identifiers are present only for model categories that own them.
  // Zero and empty values are retained rather than interpreted as missing.
  std::optional<std::string> fixtureIdText;
  std::optional<int> fixtureId;
  std::optional<int> fixtureIdNumeric;
  std::optional<int> unitNumber;
  std::optional<int> customId;
  std::optional<int> customIdType;
  std::optional<std::string> rawAddress;
  std::optional<std::string> gdtfMode;
  bool operator==(const ObjectDescriptor &) const = default;
};

struct LayerDescriptor {
  std::string uuid;
  std::string name;
  std::string color;
  std::vector<scene_identity::ObjectReference> members;
  bool operator==(const LayerDescriptor &) const = default;
};

struct GroupDescriptor {
  std::string uuid;
  std::string name;
  std::string layerUuid;
  std::string parentGroupUuid;
  std::vector<scene_identity::ObjectReference> children;
  bool operator==(const GroupDescriptor &) const = default;
};

struct PatchFixture {
  std::string uuid;
  std::string rawAddress;
  std::optional<int> universe;
  std::optional<int> startChannel;
  std::optional<int> footprint;
  std::optional<int> endChannel;
  bool operator==(const PatchFixture &) const = default;
};

struct PatchConflict {
  int universe = 0;
  int firstChannel = 0;
  int lastChannel = 0;
  std::vector<std::string> fixtureUuids;
  bool operator==(const PatchConflict &) const = default;
};

struct PatchStatus {
  std::vector<PatchFixture> fixtures;
  std::vector<PatchConflict> conflicts;
  std::vector<Diagnostic> diagnostics;
  bool operator==(const PatchStatus &) const = default;
};

} // namespace perastage::query
