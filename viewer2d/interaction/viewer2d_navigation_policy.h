/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */

#pragma once

namespace viewer2d::interaction {

struct NavigationViewState {
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float zoom = 1.0f;
};

enum class NavigationKey { Left, Right, Up, Down, Unsupported };

struct NavigationResult {
  bool handled = false;
  NavigationViewState view;
};

// Computes GUI-independent Viewer2D pan and zoom decisions.
class Viewer2DNavigationPolicy {
public:
  static constexpr float kMinimumZoom = 0.1f;
  static constexpr float kZoomBase = 1.1f;

  // Applies a framebuffer-space drag delta to the current view.
  static NavigationResult ApplyDragPan(NavigationViewState view, int deltaX,
                                       int deltaY);
  // Applies normalized mouse-wheel steps to the current zoom.
  static NavigationResult ApplyWheelZoom(NavigationViewState view, float steps);
  // Applies an arrow-key pan or Alt+arrow zoom decision.
  static NavigationResult ApplyKeyboard(NavigationViewState view,
                                        NavigationKey key, bool altDown);
};

} // namespace viewer2d::interaction
