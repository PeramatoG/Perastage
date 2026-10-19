/* This file is part of Perastage. */
#pragma once
#include "types.h"
#include <array>
#include <optional>

// Numeric source ownership follows the existing scene node, never an XML graph.
// The float projection is an exact edit sentinel, not an equivalence tolerance.
struct MvrMatrixSource {
    std::array<double, 12> values;
    Matrix projection;
};
