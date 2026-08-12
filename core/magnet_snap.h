#pragma once

#include "interactive_transform_policy.h"
#include "mvrscene.h"
#include "truss_attachment_candidates.h"
#include "truss_screen_snap.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace magnet_snap {

constexpr float kDefaultSnapDistanceMm = 250.0f;
constexpr const char *kMagnetEnabledConfigKey = "viewport_magnet_enabled";
constexpr const char *kShowAnchorReferencesConfigKey =
    "viewport_magnet_show_anchor_references";

enum class ObjectType { Fixture, Truss, TrussGroup, SceneObject };
enum class SnapKind { None, TrussToTruss, FixtureToTruss, SceneObjectToObject };

struct SnapSource {
  ObjectType type = ObjectType::SceneObject;
  std::string uuid;
};

struct SnapSettings {
  float thresholdMm = kDefaultSnapDistanceMm;
  std::array<float, 3> axisWeights{1.0f, 1.0f, 1.0f};
  truss_attachment::CandidateResolver *candidateResolver = nullptr;
  std::optional<truss_screen_snap::ProjectionSnapshot> trussProjection;
  double trussScreenApertureLogicalPx =
      truss_screen_snap::kDefaultTrussScreenSnapApertureLogicalPx;
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
  std::string sourceCandidateId;
  std::string targetCandidateId;
  std::string sourceMemberTrussUuid;
  std::string targetMemberTrussUuid;
};

struct AnchorReference {
  std::array<float, 3> positionMm{0.0f, 0.0f, 0.0f};
  std::optional<std::array<float, 3>> direction;
};

// Builds deterministic exterior or conservative aggregate group candidates.
std::vector<truss_attachment::Candidate>
BuildTrussGroupCandidates(const MvrScene &scene, const std::string &groupUuid);

// Builds every compatible anchor reference for an active Magnet source.
std::vector<AnchorReference>
BuildAnchorReferences(const MvrScene &scene, const SnapSource &source,
                      truss_attachment::CandidateResolver &resolver);

// Finds the best non-destructive Magnet snap candidate for the source object.
std::optional<SnapResult> FindSnap(const MvrScene &scene,
                                   const SnapSource &source,
                                   const SnapSettings &settings = {});

// Applies a translation-only snap result through scene_grouping transform
// helpers.
bool ApplySnapTransform(
    MvrScene &scene, const SnapResult &result,
    const scene_grouping::InteractiveTransformPolicy &policy = {});

// Creates or extends official MVR GroupObjects after a committed snap.
bool ApplyCommittedSnapGrouping(MvrScene &scene, const SnapResult &result);

// Removes the snapped source from its direct GroupObject when it is detached.
bool DetachSnapSourceFromGroup(MvrScene &scene, const SnapResult &result);

} // namespace magnet_snap
