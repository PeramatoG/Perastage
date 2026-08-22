#pragma once

#include <array>

#include "mesh.h"

namespace viewer3d::fallback {

using FixtureProjectionOutline = std::array<std::array<float, 2>, 4>;

const Mesh &FixtureCubeMesh();
const FixtureProjectionOutline &FixtureOrthographicOutline();

} // namespace viewer3d::fallback
