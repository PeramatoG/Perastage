/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "position_insertion_overlay.h"

namespace viewer2d {

std::vector<InsertionPointMarker> BuildInsertionPointMarkers(
    const position_selection::Snapshot &selection,
    const CoordinateTransform &projection) {
  std::vector<InsertionPointMarker> result;
  for (const auto &origin : selection.originsMm) {
    std::array<float, 3> meters;
    for (int axis = 0; axis < 3; ++axis)
      meters[axis] = static_cast<float>(Units::DistanceMillimetersToDisplay(
          origin[axis], Units::DistanceUnitSystem::Metric));
    const auto screen = WorldToFramebuffer(meters, projection);
    if (screen)
      result.push_back({(*screen)[0], (*screen)[1]});
  }
  return result;
}

} // namespace viewer2d
