#include "osc/osc_command_adapter.h"

#include "command/command_selection.h"
#include "command/command_transform.h"

#include <cmath>

namespace perastage::osc {
namespace {

// Creates a Command validation result for an invalid OSC mapping.
command::Result Invalid(const std::string &code, const std::string &message) {
  command::Result result;
  result.outcome = command::Outcome::ValidationError;
  result.diagnostics.push_back({command::DiagnosticSeverity::Error,
                                command::DiagnosticPhase::Validation, code,
                                message, std::nullopt});
  return result;
}

// Converts the explicit OSC transform tuple into a semantic component.
bool ReadTransform(const Message &message,
                   command::transform::Component &component) {
  if (message.arguments.size() != 5)
    return false;
  const auto *axis = std::get_if<std::string>(&message.arguments[0]);
  const auto *relative = std::get_if<bool>(&message.arguments[2]);
  const auto *space = std::get_if<std::string>(&message.arguments[3]);
  const auto *group = std::get_if<bool>(&message.arguments[4]);
  double value = 0.0;
  if (const auto *floating = std::get_if<float>(&message.arguments[1]))
    value = *floating;
  else if (const auto *integer =
               std::get_if<std::int32_t>(&message.arguments[1]))
    value = *integer;
  else
    return false;
  if (!axis || !relative || !space || !group || !std::isfinite(value))
    return false;
  if (*axis == "x")
    component.axis = 0;
  else if (*axis == "y")
    component.axis = 1;
  else if (*axis == "z")
    component.axis = 2;
  else
    return false;
  if (*space == "world")
    component.space = transform_space::TransformSpace::World;
  else if (*space == "local")
    component.space = transform_space::TransformSpace::Local;
  else
    return false;
  component.values = {value};
  component.relative = *relative;
  component.group = *group;
  return true;
}

} // namespace

// Maps and executes one supported OSC message through semantic Commands.
command::Result
ExecuteMessage(const Message &message, command::ExecutionContext &context,
               const scene_grouping::InteractiveTransformPolicy &policy) {
  if (message.address == "/perastage/selection/clear") {
    if (!message.arguments.empty())
      return Invalid("osc.invalid_arguments",
                     "Selection clear does not accept arguments.");
    return command::selection::ExecuteClear(context);
  }
  command::transform::Kind kind;
  if (message.address == "/perastage/transform/position")
    kind = command::transform::Kind::Position;
  else if (message.address == "/perastage/transform/rotation")
    kind = command::transform::Kind::Rotation;
  else
    return Invalid("osc.unsupported_address",
                   "The OSC address is not supported.");

  command::transform::Component component;
  if (!ReadTransform(message, component))
    return Invalid("osc.invalid_arguments",
                   "Transform arguments must be axis, numeric value, boolean "
                   "relative, world/local, and boolean group.");
  command::transform::Command transform{kind, {component}, std::nullopt};
  return command::transform::Execute(transform, context, policy);
}

} // namespace perastage::osc
