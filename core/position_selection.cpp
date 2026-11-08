/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "position_selection.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace position_selection {
namespace {
constexpr double kCoordinateToleranceMm = 0.0001;
}

Snapshot Read(const MvrScene &scene,
              const scene_grouping::ObjectSelection &selection,
              const scene_grouping::InteractiveTransformPolicy &policy) {
  Snapshot result;
  result.targets =
      scene_grouping::BuildInteractiveTransformTargets(scene, selection, policy);
  for (const auto &target : result.targets)
    result.originsMm.push_back(
        scene_grouping::GetTargetWorldTransform(scene, target).o);
  if (result.originsMm.empty())
    return result;
  for (int axis = 0; axis < 3; ++axis) {
    const double first = result.originsMm.front()[axis];
    if (!std::isfinite(first))
      continue;
    result.commonMm[axis] = first;
    for (const auto &origin : result.originsMm) {
      if (!std::isfinite(origin[axis]) ||
          !Units::NearlyEqualDistanceMillimeters(first, origin[axis], kCoordinateToleranceMm)) {
        result.commonMm[axis].reset();
        break;
      }
    }
  }
  return result;
}

bool SameTargets(const Snapshot &left, const Snapshot &right) {
  if (left.targets.size() != right.targets.size())
    return false;
  for (size_t i = 0; i < left.targets.size(); ++i)
    if (left.targets[i].type != right.targets[i].type ||
        left.targets[i].uuid != right.targets[i].uuid)
      return false;
  return true;
}

std::optional<double> ParseCoordinate(const std::string &text,
                                      Units::DistanceUnitSystem units) {
  const auto value = Units::ParseDistanceToMillimeters(text, units);
  if (!value || !std::isfinite(*value) ||
      std::fabs(*value) > std::numeric_limits<float>::max())
    return std::nullopt;
  return value;
}

std::string FormatCoordinate(double millimeters, Units::DistanceUnitSystem units) {
  return Units::FormatDistanceFromMillimeters(millimeters, units,
                                              Units::ValueFormatContext::Table);
}

perastage::command::Result SetAxis(
    perastage::command::ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &policy, int axis,
    const std::string &text, Units::DistanceUnitSystem units) {
  const auto value = ParseCoordinate(text, units);
  if (!value || axis < 0 || axis > 2) {
    perastage::command::Result result;
    result.outcome = perastage::command::Outcome::ValidationError;
    return result;
  }
  const auto snapshot = Read(context.scene, context.selection, policy);
  const float storedValue = static_cast<float>(*value);
  if (!snapshot.targets.empty() &&
      std::all_of(snapshot.originsMm.begin(), snapshot.originsMm.end(),
                  [&](const auto &origin) {
                    return Units::NearlyEqualDistanceMillimeters(
                        origin[axis], storedValue, kCoordinateToleranceMm);
                  }))
    return {};
  perastage::command::transform::Command command;
  command.components = {{axis, {*value}}};
  return perastage::command::transform::Execute(command, context, policy,
                                               "Position Tool");
}

} // namespace position_selection
