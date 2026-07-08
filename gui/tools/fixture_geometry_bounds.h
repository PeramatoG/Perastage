#pragma once

#include <array>
#include <string>

struct Matrix;

namespace tools {

struct FixtureGeometryBounds {
  std::array<float, 3> min = {0.0f, 0.0f, 0.0f};
  std::array<float, 3> max = {0.0f, 0.0f, 0.0f};
  bool valid = false;
};

bool ComputeFixtureGeometryBoundsMm(const std::string &gdtfPath,
                                    FixtureGeometryBounds &bounds,
                                    std::string &errorMessage);

} // namespace tools
