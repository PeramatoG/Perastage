/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#pragma once

#include "command/command_transform.h"
#include "units/units.h"

#include <array>
#include <optional>
#include <string>

namespace position_selection {

// Values remain in scene millimetres; nullopt means a mixed axis.
struct Snapshot {
  std::vector<scene_grouping::SceneTransformTarget> targets;
  std::vector<std::array<float, 3>> originsMm;
  std::array<std::optional<double>, 3> commonMm;
};

Snapshot Read(const MvrScene &scene,
              const scene_grouping::ObjectSelection &selection,
              const scene_grouping::InteractiveTransformPolicy &policy);
bool SameTargets(const Snapshot &left, const Snapshot &right);
std::optional<double> ParseCoordinate(const std::string &text,
                                      Units::DistanceUnitSystem units);
std::string FormatCoordinate(double millimeters, Units::DistanceUnitSystem units);

// Executes one absolute world-axis edit through the existing command transaction.
perastage::command::Result SetAxis(
    perastage::command::ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &policy, int axis,
    const std::string &text, Units::DistanceUnitSystem units);

// Captures the existing authoritative selection, including directly selected groups.
template <typename Store>
scene_grouping::ObjectSelection SelectionFrom(const Store &store) {
  return {store.GetSelectedFixtures(), store.GetSelectedTrusses(),
          store.GetSelectedSupports(), store.GetSelectedSceneObjects(),
          store.GetSelectedGroupObjects()};
}

} // namespace position_selection
