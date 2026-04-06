#pragma once

#include "viewer3d_types.h"

#include <string>

bool TryGetPrimitiveBoundsFromModelRef(const std::string &modelRef,
                                       Viewer3DBoundingBox &outBounds);
