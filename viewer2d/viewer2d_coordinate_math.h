#pragma once

#include "../viewer3d/viewer3d_types.h"

#include <array>
#include <optional>

namespace viewer2d {

// Describes the framebuffer-space orthographic projection used by Viewer2D.
struct CoordinateTransform {
  int framebufferWidth = 0;
  int framebufferHeight = 0;
  float zoom = 1.0f;
  float panPixelsX = 0.0f;
  float panPixelsY = 0.0f;
  Viewer2DView view = Viewer2DView::Top;
};

// Stores the visible world-metre bounds for an OpenGL orthographic projection.
struct OrthographicBounds {
  float left = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  float top = 0.0f;
};

// Defines the visible world axes and their signs on the rendered screen.
struct ViewBasis {
  int horizontalAxis = 0;
  int verticalAxis = 1;
  float horizontalSign = 1.0f;
  float verticalSign = 1.0f;
};

constexpr float kPixelsPerMeter = 25.0f;

// Returns the authoritative screen basis established by the OpenGL camera.
ViewBasis GetViewBasis(Viewer2DView view);

// Maps a screen-space metre delta onto the visible world axes.
std::array<float, 3> ScreenDeltaToWorld(float horizontalMeters,
                                        float verticalMeters,
                                        Viewer2DView view);

// Computes projection bounds from the same transform used for pointer mapping.
std::optional<OrthographicBounds>
ComputeOrthographicBounds(const CoordinateTransform &transform);

// Projects a world position to top-origin physical framebuffer pixels.
std::optional<std::array<float, 2>>
WorldToFramebuffer(const std::array<float, 3> &world,
                   const CoordinateTransform &transform);

// Unprojects top-origin physical framebuffer pixels onto the active view plane.
std::optional<std::array<float, 3>>
FramebufferToWorld(const std::array<float, 2> &framebuffer,
                   const CoordinateTransform &transform);

} // namespace viewer2d
