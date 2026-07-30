#include "viewer2d_coordinate_math.h"

#include <cmath>

namespace viewer2d {
namespace {

// Reports whether a coordinate transform can produce finite inverse mappings.
bool IsValid(const CoordinateTransform &transform) {
  return transform.framebufferWidth > 0 && transform.framebufferHeight > 0 &&
         transform.zoom > 0.0f && std::isfinite(transform.zoom) &&
         std::isfinite(transform.panPixelsX) &&
         std::isfinite(transform.panPixelsY);
}

// Maps a world position onto the signed axes of the rendered view camera.
std::array<float, 2> VisibleAxes(const std::array<float, 3> &world,
                                 Viewer2DView view) {
  const ViewBasis basis = GetViewBasis(view);
  return {world[basis.horizontalAxis] * basis.horizontalSign,
          world[basis.verticalAxis] * basis.verticalSign};
}

} // namespace

// Returns the authoritative screen basis established by the OpenGL camera.
ViewBasis GetViewBasis(Viewer2DView view) {
  switch (view) {
  case Viewer2DView::Top:
    return {0, 1, 1.0f, 1.0f};
  case Viewer2DView::Bottom:
    return {0, 1, -1.0f, 1.0f};
  case Viewer2DView::Front:
    return {0, 2, 1.0f, 1.0f};
  case Viewer2DView::Side:
    return {1, 2, -1.0f, 1.0f};
  }
  return {0, 1, 1.0f, 1.0f};
}

// Maps a screen-space metre delta onto the visible world axes.
std::array<float, 3> ScreenDeltaToWorld(float horizontalMeters,
                                        float verticalMeters,
                                        Viewer2DView view) {
  const ViewBasis basis = GetViewBasis(view);
  std::array<float, 3> world{0.0f, 0.0f, 0.0f};
  world[basis.horizontalAxis] = horizontalMeters * basis.horizontalSign;
  world[basis.verticalAxis] = verticalMeters * basis.verticalSign;
  return world;
}

// Computes projection bounds from the same transform used for pointer mapping.
std::optional<OrthographicBounds>
ComputeOrthographicBounds(const CoordinateTransform &transform) {
  if (!IsValid(transform))
    return std::nullopt;
  const float scale = kPixelsPerMeter * transform.zoom;
  const float halfWidth = transform.framebufferWidth * 0.5f / scale;
  const float halfHeight = transform.framebufferHeight * 0.5f / scale;
  const float panX = transform.panPixelsX / kPixelsPerMeter;
  const float panY = transform.panPixelsY / kPixelsPerMeter;
  return OrthographicBounds{-halfWidth - panX, halfWidth - panX,
                            -halfHeight - panY, halfHeight - panY};
}

// Projects a world position to top-origin physical framebuffer pixels.
std::optional<std::array<float, 2>>
WorldToFramebuffer(const std::array<float, 3> &world,
                   const CoordinateTransform &transform) {
  if (!IsValid(transform))
    return std::nullopt;
  const auto visible = VisibleAxes(world, transform.view);
  return std::array<float, 2>{
      transform.framebufferWidth * 0.5f +
          (visible[0] * kPixelsPerMeter + transform.panPixelsX) *
              transform.zoom,
      transform.framebufferHeight * 0.5f -
          (visible[1] * kPixelsPerMeter + transform.panPixelsY) *
              transform.zoom};
}

// Unprojects top-origin physical framebuffer pixels onto the active view plane.
std::optional<std::array<float, 3>>
FramebufferToWorld(const std::array<float, 2> &framebuffer,
                   const CoordinateTransform &transform) {
  if (!IsValid(transform))
    return std::nullopt;
  const float scale = kPixelsPerMeter * transform.zoom;
  const float u = (framebuffer[0] - transform.framebufferWidth * 0.5f) / scale -
                  transform.panPixelsX / kPixelsPerMeter;
  const float v =
      (transform.framebufferHeight * 0.5f - framebuffer[1]) / scale -
      transform.panPixelsY / kPixelsPerMeter;
  return ScreenDeltaToWorld(u, v, transform.view);
}

} // namespace viewer2d
