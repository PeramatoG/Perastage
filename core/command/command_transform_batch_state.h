#pragma once

#include "scene_grouping.h"

namespace perastage::command::transform::detail {

// Includes exact targets and recursively synchronized GroupObject descendants.
std::vector<scene_grouping::SceneTransformTarget> CollectAffectedTargets(
    const MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets);

// Rejects non-finite world/local state produced by any preview component.
void RequireFiniteTransforms(
    const MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets);

// Compares final affected world transforms and meaningful grouping local state.
bool BatchTransformsChanged(
    const MvrScene &before, const MvrScene &after,
    const std::vector<scene_grouping::SceneTransformTarget> &targets);

} // namespace perastage::command::transform::detail
