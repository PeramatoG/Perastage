/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */

#include "viewer2d_navigation_policy.h"

#include <algorithm>
#include <cmath>

namespace viewer2d::interaction {

// Applies a framebuffer-space drag delta to the current view.
NavigationResult
Viewer2DNavigationPolicy::ApplyDragPan(NavigationViewState view, int deltaX,
                                       int deltaY) {
  view.offsetX += static_cast<float>(deltaX) / view.zoom;
  view.offsetY += static_cast<float>(deltaY) / view.zoom;
  return {deltaX != 0 || deltaY != 0, view};
}

// Applies normalized mouse-wheel steps to the current zoom.
NavigationResult
Viewer2DNavigationPolicy::ApplyWheelZoom(NavigationViewState view,
                                         float steps) {
  view.zoom = std::max(kMinimumZoom, view.zoom * std::pow(kZoomBase, steps));
  return {true, view};
}

// Applies an arrow-key pan or Alt+arrow zoom decision.
NavigationResult
Viewer2DNavigationPolicy::ApplyKeyboard(NavigationViewState view,
                                        NavigationKey key, bool altDown) {
  if (key == NavigationKey::Unsupported)
    return {false, view};

  const float panStep = 10.0f / view.zoom;
  switch (key) {
  case NavigationKey::Left:
    altDown ? view.zoom *= kZoomBase : view.offsetX += panStep;
    break;
  case NavigationKey::Right:
    altDown ? view.zoom /= kZoomBase : view.offsetX -= panStep;
    break;
  case NavigationKey::Up:
    altDown ? view.zoom *= kZoomBase : view.offsetY -= panStep;
    break;
  case NavigationKey::Down:
    altDown ? view.zoom /= kZoomBase : view.offsetY += panStep;
    break;
  case NavigationKey::Unsupported:
    return {false, view};
  }
  view.zoom = std::max(kMinimumZoom, view.zoom);
  return {true, view};
}

} // namespace viewer2d::interaction
