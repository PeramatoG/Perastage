#include "fixture_fallback_visual.h"

namespace viewer3d::fallback {

// Returns the canonical Perastage-owned emergency fixture cube.
const Mesh &FixtureCubeMesh() {
  static const Mesh mesh = []() {
    Mesh cube;
    cube.vertices = {
        -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f,
        0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f,
        -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,
        0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  0.5f};
    cube.indices = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
                    0, 4, 5, 0, 5, 1, 3, 2, 6, 3, 6, 7,
                    0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2};
    ComputeNormals(cube);
    return cube;
  }();
  return mesh;
}

// Returns the deterministic square shared by every orthographic cube projection.
const FixtureProjectionOutline &FixtureOrthographicOutline() {
  static constexpr FixtureProjectionOutline outline = {
      std::array<float, 2>{-0.5f, -0.5f}, {0.5f, -0.5f},
      {0.5f, 0.5f}, {-0.5f, 0.5f}};
  return outline;
}

} // namespace viewer3d::fallback
