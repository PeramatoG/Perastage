#include "interaction_lod_policy.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <algorithm>
#include <array>
#include <cfloat>

namespace {

struct ScreenRect {
  double minX = DBL_MAX;
  double minY = DBL_MAX;
  double maxX = -DBL_MAX;
  double maxY = -DBL_MAX;
};

bool ProjectBoundingBoxToScreen(const Viewer3DBoundingBox &bb,
                                const Viewer3DViewFrustumSnapshot &frustum,
                                ScreenRect &outRect) {
  outRect = ScreenRect{};
  bool projectedAny = false;
  const int viewportHeight = frustum.viewport[3];

  std::array<std::array<float, 3>, 8> corners = {
      std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
      {bb.max[0], bb.min[1], bb.min[2]},
      {bb.min[0], bb.max[1], bb.min[2]},
      {bb.max[0], bb.max[1], bb.min[2]},
      {bb.min[0], bb.min[1], bb.max[2]},
      {bb.max[0], bb.min[1], bb.max[2]},
      {bb.min[0], bb.max[1], bb.max[2]},
      {bb.max[0], bb.max[1], bb.max[2]}};

  for (const auto &corner : corners) {
    double sx = 0.0;
    double sy = 0.0;
    double sz = 0.0;
    if (gluProject(corner[0], corner[1], corner[2], frustum.model,
                   frustum.projection, frustum.viewport, &sx, &sy,
                   &sz) != GL_TRUE) {
      continue;
    }
    projectedAny = true;
    outRect.minX = std::min(outRect.minX, sx);
    outRect.maxX = std::max(outRect.maxX, sx);
    const double yFlipped = static_cast<double>(viewportHeight) - sy;
    outRect.minY = std::min(outRect.minY, yFlipped);
    outRect.maxY = std::max(outRect.maxY, yFlipped);
  }

  return projectedAny;
}

} // namespace

bool ShouldUseInteractionProxy(
    const InteractionLodPolicy &policy,
    const Viewer3DViewFrustumSnapshot *frustum,
    const Viewer3DBoundingBox *worldBounds,
    size_t triangleCount) {
  if (!policy.enabled)
    return false;

  const bool isHeavy = triangleCount >= policy.heavyMeshTriangleThreshold;
  if (!frustum || !worldBounds)
    return isHeavy;

  ScreenRect rect;
  if (!ProjectBoundingBoxToScreen(*worldBounds, *frustum, rect))
    return isHeavy;

  const double widthPixels = std::max(0.0, rect.maxX - rect.minX);
  const double heightPixels = std::max(0.0, rect.maxY - rect.minY);
  const bool tinyOnScreen =
      widthPixels < policy.minScreenPixels &&
      heightPixels < policy.minScreenPixels;

  return isHeavy || tinyOnScreen;
}
