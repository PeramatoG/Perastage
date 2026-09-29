#include "command_transform_text_adapter.h"

namespace perastage::command::text {

// Converts neutral Console transform syntax to explicit semantic units.
transform::Command AdaptTransform(const TransformCommand &command) {
  transform::Command semantic;
  semantic.kind = command.kind == TransformKind::Position
                      ? transform::Kind::Position
                      : transform::Kind::Rotation;
  semantic.pivotMm =
      command.pivotMm
          ? std::optional<std::array<double, 3>>{{(*command.pivotMm)[0],
                                                  (*command.pivotMm)[1],
                                                  (*command.pivotMm)[2]}}
          : std::nullopt;
  for (const TransformComponent &parsed : command.components) {
    transform::Component component;
    component.axis = parsed.axis;
    component.relative = parsed.relative;
    component.group = parsed.group;
    component.space = parsed.space;
    for (float value : parsed.values)
      component.values.push_back(
          command.kind == TransformKind::Position ? value * 1000.0 : value);
    semantic.components.push_back(std::move(component));
  }
  return semantic;
}

} // namespace perastage::command::text
