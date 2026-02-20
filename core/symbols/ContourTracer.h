#pragma once

#include "symbols/MaskUtils.h"
#include "symbols/Symbol2DTypes.h"

#include <vector>

namespace symbols {

std::vector<PolygonWithHoles2D> TraceFillPolygons(const BinaryMask &mask,
                                                  int width,
                                                  int height,
                                                  float simplifyTolerance);

} // namespace symbols
