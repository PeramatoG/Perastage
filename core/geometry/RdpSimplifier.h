#pragma once

#include <vector>

#include "symbols/Symbol2DTypes.h"

namespace geometry {

std::vector<symbols::Point2D>
CanonicalizePolygonRing(const std::vector<symbols::Point2D> &pts,
                        bool explicitlyClosed = false);

std::vector<symbols::Point2D>
SimplifyRdpPolyline(const std::vector<symbols::Point2D> &pts, float epsilon);

std::vector<symbols::Point2D>
SimplifyRdpPolygonClosed(const std::vector<symbols::Point2D> &pts, float epsilon);

} // namespace geometry
