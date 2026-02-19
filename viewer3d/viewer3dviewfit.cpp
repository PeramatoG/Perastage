#include "viewer3dviewfit.h"

#include "viewer3dcamera.h"
#include "iselectioncontext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr float kFitPaddingScale = 1.03f;
constexpr float kNearPlaneSafety = 0.1f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kFovYDegrees = 45.0f;

std::array<float, 3> Normalize(const std::array<float, 3> &v) {
  const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len <= 1e-6f)
    return {0.0f, 1.0f, 0.0f};
  return {v[0] / len, v[1] / len, v[2] / len};
}

} // namespace

namespace viewer3d {

bool FrameSceneInCamera(const ISelectionContext &selectionContext,
                        int viewportWidth, int viewportHeight,
                        Viewer3DCamera &camera) {
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

  accumulate(selectionContext.GetFixtureBoundsMap());
  accumulate(selectionContext.GetTrussBoundsMap());
  accumulate(selectionContext.GetObjectBoundsMap());

  if (minX > maxX || minY > maxY || minZ > maxZ)
    return false;

  const std::array<float, 3> center = {
      (minX + maxX) * 0.5f,
      (minY + maxY) * 0.5f,
      (minZ + maxZ) * 0.5f,
  };

  const float yawRad = camera.GetYaw() * kPi / 180.0f;
  const float pitchRad = camera.GetPitch() * kPi / 180.0f;

  const std::array<float, 3> cameraOffset = {
      std::cos(pitchRad) * std::sin(yawRad),
      -std::cos(pitchRad) * std::cos(yawRad),
      std::sin(pitchRad),
  };
  const std::array<float, 3> forward =
      Normalize({-cameraOffset[0], -cameraOffset[1], -cameraOffset[2]});
  const std::array<float, 3> worldUp = {0.0f, 0.0f, 1.0f};
  const std::array<float, 3> right =
      Normalize({forward[1] * worldUp[2] - forward[2] * worldUp[1],
                 forward[2] * worldUp[0] - forward[0] * worldUp[2],
                 forward[0] * worldUp[1] - forward[1] * worldUp[0]});
  const std::array<float, 3> up =
      Normalize({right[1] * forward[2] - right[2] * forward[1],
                 right[2] * forward[0] - right[0] * forward[2],
                 right[0] * forward[1] - right[1] * forward[0]});

  const float aspect =
      static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
  const float fovY = kFovYDegrees * kPi / 180.0f;
  const float tanHalfFovY = std::tan(fovY * 0.5f);
  const float tanHalfFovX = tanHalfFovY * aspect;

  std::array<std::array<float, 3>, 8> corners = {{
      {minX, minY, minZ},
      {minX, minY, maxZ},
      {minX, maxY, minZ},
      {minX, maxY, maxZ},
      {maxX, minY, minZ},
      {maxX, minY, maxZ},
      {maxX, maxY, minZ},
      {maxX, maxY, maxZ},
  }};

  float requiredDistance = 0.0f;
  for (const auto &corner : corners) {
    const std::array<float, 3> rel = {corner[0] - center[0],
                                      corner[1] - center[1],
                                      corner[2] - center[2]};

    const float x = rel[0] * right[0] + rel[1] * right[1] + rel[2] * right[2];
    const float y = rel[0] * up[0] + rel[1] * up[1] + rel[2] * up[2];
    const float z =
        rel[0] * forward[0] + rel[1] * forward[1] + rel[2] * forward[2];

    requiredDistance =
        std::max(requiredDistance, std::abs(x) / std::max(0.01f, tanHalfFovX) - z);
    requiredDistance =
        std::max(requiredDistance, std::abs(y) / std::max(0.01f, tanHalfFovY) - z);
    requiredDistance = std::max(requiredDistance, kNearPlaneSafety - z);
  }

  requiredDistance = std::max(requiredDistance * kFitPaddingScale, 0.2f);

  camera.SetTarget(center[0], center[1], center[2]);
  camera.SetDistance(requiredDistance);
  return true;
}

} // namespace viewer3d
