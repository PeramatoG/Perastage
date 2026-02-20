#pragma once

#include "symbols/Symbol2DTypes.h"

#include <vector>

namespace symbols {

std::vector<Point2D> SimplifyRdp(const std::vector<Point2D> &points,
                                 float tolerance,
                                 bool closed);

} // namespace symbols
