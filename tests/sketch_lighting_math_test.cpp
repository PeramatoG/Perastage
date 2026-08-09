#include "mesh.h"
#include "render/sketch_lighting_math.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {

// Verifies two vectors are equal within floating-point tolerance.
void AssertVectorNear(const std::array<float, 3> &actual,
                      const std::array<float, 3> &expected) {
  constexpr float kTolerance = 0.0001f;
  for (size_t i = 0; i < actual.size(); ++i)
    assert(std::fabs(actual[i] - expected[i]) < kTolerance);
}

// Applies a column-major 3x3 matrix as the Sketch GPU shader does.
std::array<float, 3> ApplyGpuNormalMatrix(const float matrix[9],
                                          const std::array<float, 3> &normal) {
  const float x =
      matrix[0] * normal[0] + matrix[3] * normal[1] + matrix[6] * normal[2];
  const float y =
      matrix[1] * normal[0] + matrix[4] * normal[1] + matrix[7] * normal[2];
  const float z =
      matrix[2] * normal[0] + matrix[5] * normal[1] + matrix[8] * normal[2];
  const float length = std::sqrt(x * x + y * y + z * z);
  return {x / length, y / length, z / length};
}

// Builds the same inverse-transpose matrix uploaded by the Sketch GPU path.
void BuildGpuNormalMatrix(const float model[16], float result[9]) {
  const float a00 = model[0], a01 = model[4], a02 = model[8];
  const float a10 = model[1], a11 = model[5], a12 = model[9];
  const float a20 = model[2], a21 = model[6], a22 = model[10];
  const float c00 = a11 * a22 - a12 * a21;
  const float c01 = a12 * a20 - a10 * a22;
  const float c02 = a10 * a21 - a11 * a20;
  const float c10 = a02 * a21 - a01 * a22;
  const float c11 = a00 * a22 - a02 * a20;
  const float c12 = a01 * a20 - a00 * a21;
  const float c20 = a01 * a12 - a02 * a11;
  const float c21 = a02 * a10 - a00 * a12;
  const float c22 = a00 * a11 - a01 * a10;
  const float invDet = 1.0f / (a00 * c00 + a01 * c01 + a02 * c02);
  const float values[9] = {c00, c10, c20, c01, c11, c21, c02, c12, c22};
  for (size_t i = 0; i < 9; ++i)
    result[i] = values[i] * invDet;
}

} // namespace

// Verifies Sketch normal transforms and two-sided orientation without OpenGL.
int main() {
  const float rotateZ90[16] = {0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                               0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f};
  AssertVectorNear(TransformNormal({1.0f, 0.0f, 0.0f}, rotateZ90),
                   {0.0f, 1.0f, 0.0f});

  const float scaledRotation[16] = {0.0f, 2.0f, 0.0f, 0.0f, -3.0f, 0.0f,
                                    0.0f, 0.0f, 0.0f, 0.0f, 4.0f,  0.0f,
                                    0.0f, 0.0f, 0.0f, 1.0f};
  const auto cpuNormal = TransformNormal({1.0f, 1.0f, 1.0f}, scaledRotation);
  constexpr float expectedLength = 0.65085414f;
  AssertVectorNear(cpuNormal, {-1.0f / 3.0f / expectedLength,
                               0.5f / expectedLength,
                               0.25f / expectedLength});
  float gpuNormalMatrix[9];
  BuildGpuNormalMatrix(scaledRotation, gpuNormalMatrix);
  AssertVectorNear(ApplyGpuNormalMatrix(gpuNormalMatrix, {1.0f, 1.0f, 1.0f}),
                   cpuNormal);

  const float mirrored[16] = {-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                              0.0f,  0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  assert(TransformDeterminant(mirrored) < 0.0f);

  const std::array<float, 3> normal = {0.0f, 0.0f, 1.0f};
  AssertVectorNear(
      Viewer3DSketchLighting::OrientNormalForFace(normal, true, true), normal);
  AssertVectorNear(
      Viewer3DSketchLighting::OrientNormalForFace(normal, false, true),
      {0.0f, 0.0f, -1.0f});
  AssertVectorNear(
      Viewer3DSketchLighting::OrientNormalForFace(normal, false, false),
      normal);
  return 0;
}
