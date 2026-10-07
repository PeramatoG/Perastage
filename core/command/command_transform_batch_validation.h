#pragma once

#include "command/command_transform.h"

namespace perastage::command::transform::detail {

struct BatchRow {
  scene_grouping::SceneTransformTarget target;
  Kind kind = Kind::Position;
  Component component;
};

// Resolves and validates every row without changing the supplied scene.
bool ValidateBatch(const Request &request, const MvrScene &scene,
                   std::vector<BatchRow> &rows, Result &result);

} // namespace perastage::command::transform::detail
