#pragma once

#include "command/command_execution.h"
#include "interactive_transform_policy.h"
#include "transform_space.h"

#include <array>
#include <optional>
#include <vector>

namespace perastage::command::transform {

inline constexpr char kPositionCommandId[] = "scene.transform.position";
inline constexpr char kRotationCommandId[] = "scene.transform.rotation";

enum class Kind { Position, Rotation };

struct Component {
  int axis = 0;
  std::vector<double> values;
  bool relative = false;
  bool group = false;
  transform_space::TransformSpace space =
      transform_space::TransformSpace::World;
};

struct Command {
  Kind kind = Kind::Position;
  std::vector<Component> components;
  std::optional<std::array<double, 3>> pivotMm;
};

// Projects a typed transform command onto the stable generic request contract.
Request BuildRequest(const Command &command);

// Validates and executes one semantic transform as one mutation transaction.
Result Execute(const Command &command, ExecutionContext &context,
               const scene_grouping::InteractiveTransformPolicy &policy);

} // namespace perastage::command::transform
