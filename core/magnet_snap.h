#pragma once

#include "mvrscene.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace magnet_snap {

constexpr float kDefaultSnapDistanceMm = 250.0f;
constexpr const char *kMagnetEnabledConfigKey = "viewport_magnet_enabled";

enum class ObjectType { Fixture, Truss, TrussGroup, SceneObject };
enum class SnapKind { None, TrussToTruss, FixtureToTruss, SceneObjectToObject };

struct SnapSource {
  ObjectType type = ObjectType::SceneObject;
  std::string uuid;
};

struct SnapSettings {
  float thresholdMm = kDefaultSnapDistanceMm;
  std::array<float, 3> axisWeights{1.0f, 1.0f, 1.0f};
};

struct SnapResult {
  bool snapped = false;
  SnapKind kind = SnapKind::None;
  std::string sourceUuid;
  std::string targetUuid;
  ObjectType sourceType = ObjectType::SceneObject;
  ObjectType targetType = ObjectType::SceneObject;
  std::array<float, 3> translationDeltaMm{0.0f, 0.0f, 0.0f};
  bool needsGrouping = false;
};

// Finds the best non-destructive Magnet snap candidate for the source object.
std::optional<SnapResult> FindSnap(const MvrScene &scene,
                                   const SnapSource &source,
                                   const SnapSettings &settings = {});

// Applies a translation-only snap result through scene_grouping transform helpers.
bool ApplySnapTransform(MvrScene &scene, const SnapResult &result);

// Creates or extends official MVR GroupObjects after a committed snap.
bool ApplyCommittedSnapGrouping(MvrScene &scene, const SnapResult &result);

// Removes the snapped source from its direct GroupObject when it is detached.
bool DetachSnapSourceFromGroup(MvrScene &scene, const SnapResult &result);

} // namespace magnet_snap
