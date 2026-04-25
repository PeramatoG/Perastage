#pragma once

#include "viewer3d_types.h"

#include <cstddef>

struct InteractionLodPolicy {
  bool enabled = false;
  float minScreenPixels = 4.0f;
  size_t heavyMeshTriangleThreshold = 4000;
};

bool ShouldUseInteractionProxy(
    const InteractionLodPolicy &policy,
    const Viewer3DViewFrustumSnapshot *frustum,
    const Viewer3DBoundingBox *worldBounds,
    size_t triangleCount);
