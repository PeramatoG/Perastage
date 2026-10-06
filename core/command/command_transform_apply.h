#pragma once

#include "command/command_transform.h"

namespace perastage::command::transform::detail {

// Applies the shared position/rotation semantics to already resolved targets.
void ApplyComponent(
    MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets, Kind kind,
    const Component &component);

} // namespace perastage::command::transform::detail
