#pragma once

#include "symbols/MaskUtils.h"
#include "symbols/Symbol2DTypes.h"

#include <vector>

namespace symbols {

std::vector<Polyline2D> SkeletonToPolylines(const BinaryMask &skeleton,
                                            int width,
                                            int height,
                                            float minLength,
                                            float simplifyTolerance);

} // namespace symbols
