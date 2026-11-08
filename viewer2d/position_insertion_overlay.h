/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#pragma once

#include "position_selection.h"
#include "viewer2d_coordinate_math.h"

namespace viewer2d {

// Presentation pixels only: never submitted to a capture canvas or scene renderer.
struct InsertionPointMarker {
  float x = 0.0f;
  float y = 0.0f;
};

inline bool ShowInsertionPointOverlay(bool onscreenFrame, bool selectionEnabled,
                                      bool captureActive, bool offscreen) {
  return onscreenFrame && selectionEnabled && !captureActive && !offscreen;
}

std::vector<InsertionPointMarker> BuildInsertionPointMarkers(
    const position_selection::Snapshot &selection,
    const CoordinateTransform &projection);

} // namespace viewer2d
