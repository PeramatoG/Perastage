#pragma once

#include "mesh.h"

#include <array>
#include <unordered_map>
#include <vector>

struct FixtureInstanceDrawData {
  std::array<float, 16> modelMatrix{};
  std::array<float, 3> color{};
};

using FixtureInstancedBatches =
    std::unordered_map<const Mesh *, std::vector<FixtureInstanceDrawData>>;

bool RenderFixtureInstancedBatches(const FixtureInstancedBatches &batches);
