#include "viewer3dviewfit.h"

#include "viewer3dcamera.h"
#include "viewer3dcontroller.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kMinRadiusMeters = 0.25f;
constexpr float kFitPaddingScale = 1.2f;
constexpr float kFovYDegrees = 45.0f;

} // namespace

namespace viewer3d {

bool FrameSceneInCamera(const Viewer3DController &controller, int viewportWidth,
                        int viewportHeight, Viewer3DCamera &camera) {
  if (viewportWidth <= 0 || viewportHeight <= 0)
    return false;

  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float minZ = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  float maxZ = std::numeric_limits<float>::lowest();

  auto accumulate = [&](const auto &map) {
    for (const auto &[uuid, bounds] : map) {
      (void)uuid;
      minX = std::min(minX, bounds.min[0]);
      minY = std::min(minY, bounds.min[1]);
      minZ = std::min(minZ, bounds.min[2]);
      maxX = std::max(maxX, bounds.max[0]);
      maxY = std::max(maxY, bounds.max[1]);
      maxZ = std::max(maxZ, bounds.max[2]);
    }
  };

  accumulate(controller.GetFixtureBoundsMap());
  accumulate(controller.GetTrussBoundsMap());
  accumulate(controller.GetObjectBoundsMap());

  if (minX > maxX || minY > maxY || minZ > maxZ)
    return false;

  const float centerX = (minX + maxX) * 0.5f;
  const float centerY = (minY + maxY) * 0.5f;
  const float centerZ = (minZ + maxZ) * 0.5f;

  const float halfX = (maxX - minX) * 0.5f;
  const float halfY = (maxY - minY) * 0.5f;
  const float halfZ = (maxZ - minZ) * 0.5f;
  const float radius =
      std::max(kMinRadiusMeters, std::sqrt(halfX * halfX + halfY * halfY + halfZ * halfZ));

  const float aspect =
      static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
  const float fovY = kFovYDegrees * static_cast<float>(M_PI) / 180.0f;
  const float fovX = 2.0f * std::atan(std::tan(fovY * 0.5f) * aspect);
  const float limitingHalfFov = std::min(fovX, fovY) * 0.5f;

  const float requiredDistance =
      (radius * kFitPaddingScale) / std::max(0.01f, std::sin(limitingHalfFov));

  camera.SetTarget(centerX, centerY, centerZ);
  camera.SetDistance(requiredDistance);
  return true;
}

} // namespace viewer3d
