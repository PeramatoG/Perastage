#pragma once

#include <array>
#include <cmath>

namespace Viewer3DMeshShading {

enum class Mode { Flat, Smooth };

// Maps the render pass flat-shading request to the shared mesh shading mode.
inline Mode ResolveMode(bool flatShadingRequested) {
  return flatShadingRequested ? Mode::Flat : Mode::Smooth;
}

// Computes the normalized geometric normal used by flat mesh shading.
inline std::array<float, 3>
ComputeFaceNormal(const std::array<float, 3> &first,
                  const std::array<float, 3> &second,
                  const std::array<float, 3> &third) {
  const float ux = second[0] - first[0];
  const float uy = second[1] - first[1];
  const float uz = second[2] - first[2];
  const float vx = third[0] - first[0];
  const float vy = third[1] - first[1];
  const float vz = third[2] - first[2];
  const float nx = uy * vz - uz * vy;
  const float ny = uz * vx - ux * vz;
  const float nz = ux * vy - uy * vx;
  const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
  if (length <= 1e-6f)
    return {0.0f, 0.0f, 1.0f};
  return {nx / length, ny / length, nz / length};
}

// Selects the same local normal source for Standard and Sketch mesh shading.
inline std::array<float, 3>
SelectNormal(Mode mode, const std::array<float, 3> &geometricFaceNormal,
             const std::array<float, 3> &vertexNormal,
             bool hasValidVertexNormal) {
  if (mode == Mode::Smooth && hasValidVertexNormal)
    return vertexNormal;
  return geometricFaceNormal;
}

} // namespace Viewer3DMeshShading
