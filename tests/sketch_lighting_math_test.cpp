#include "mesh.h"
#include "render/lighting_profile.h"
#include "render/mesh_shading_policy.h"
#include "render/sketch_lighting_math.h"

#include <algorithm>
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

// Returns the dot product of two directions.
float Dot(const std::array<float, 3> &left,
          const std::array<float, 3> &right) {
  return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

// Reproduces the combined diffuse expression evaluated by the GPU shader.
float ShaderCombinedDiffuse(
    const std::array<float, 3> &normal,
    const Viewer3DLightingProfile::LightingState &lightingState) {
  const float key = lightingState.keyDiffuseWeight *
                    std::max(Dot(normal, lightingState.keyLightEyeDirection),
                             0.0f);
  const float fill = lightingState.fillDiffuseWeight *
                     std::max(Dot(normal, lightingState.fillLightEyeDirection),
                              0.0f);
  const float weightSum =
      lightingState.keyDiffuseWeight + lightingState.fillDiffuseWeight;
  return weightSum > 0.000001f ? (key + fill) / weightSum : 0.0f;
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
  const auto gpuNormal =
      ApplyGpuNormalMatrix(gpuNormalMatrix, {1.0f, 1.0f, 1.0f});
  AssertVectorNear(gpuNormal, cpuNormal);

  const float mirrored[16] = {-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                              0.0f,  0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  assert(TransformDeterminant(mirrored) < 0.0f);
  AssertVectorNear(TransformNormal({1.0f, 0.0f, 0.0f}, mirrored),
                   {-1.0f, 0.0f, 0.0f});

  const std::array<float, 3> normal = {0.0f, 0.0f, 1.0f};
  AssertVectorNear(
      Viewer3DSketchLighting::OrientNormalForFace(normal, true, true), normal);
  AssertVectorNear(
      Viewer3DSketchLighting::OrientNormalForFace(normal, false, true),
      {0.0f, 0.0f, -1.0f});
  AssertVectorNear(
      Viewer3DSketchLighting::OrientNormalForFace(normal, false, false),
      normal);

  const float identity[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                              0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                              0.0f, 0.0f, 0.0f, 1.0f};
  const auto worldLight = Viewer3DLightingProfile::NormalizeDirection(
      Viewer3DLightingProfile::kKeyLightWorldDirection);
  const auto worldFillLight = Viewer3DLightingProfile::NormalizeDirection(
      Viewer3DLightingProfile::kFillLightWorldDirection);
  AssertVectorNear(Viewer3DLightingProfile::TransformWorldDirectionToEyeSpace(
                       Viewer3DLightingProfile::kKeyLightWorldDirection,
                       identity),
                   worldLight);
  AssertVectorNear(Viewer3DLightingProfile::TransformWorldDirectionToEyeSpace(
                       Viewer3DLightingProfile::kFillLightWorldDirection,
                       identity),
                   worldFillLight);

  Viewer3DLightingProfile::LightingState worldLighting;
  worldLighting.keyLightEyeDirection = worldLight;
  worldLighting.fillLightEyeDirection = worldFillLight;
  worldLighting.keyDiffuseWeight = 0.78f;
  worldLighting.fillDiffuseWeight = 0.32f;
  assert(std::fabs(Viewer3DLightingProfile::CombinedDirectionalDiffuse(
                       cpuNormal, worldLighting) -
                   Viewer3DLightingProfile::CombinedDirectionalDiffuse(
                       gpuNormal, worldLighting)) < 0.0001f);

  const auto betweenLights = Viewer3DLightingProfile::NormalizeDirection(
      {worldLight[0] + worldFillLight[0],
       worldLight[1] + worldFillLight[1],
       worldLight[2] + worldFillLight[2]});
  const std::array<float, 3> awayFromBoth = {-betweenLights[0],
                                             -betweenLights[1],
                                             -betweenLights[2]};
  assert(Viewer3DLightingProfile::CombinedDirectionalDiffuse(worldLight,
                                                              worldLighting) >
         0.5f);
  assert(Viewer3DLightingProfile::CombinedDirectionalDiffuse(worldFillLight,
                                                              worldLighting) >
         0.1f);
  assert(Viewer3DLightingProfile::CombinedDirectionalDiffuse(betweenLights,
                                                              worldLighting) >
         0.3f);
  assert(Viewer3DLightingProfile::CombinedDirectionalDiffuse(awayFromBoth,
                                                              worldLighting) ==
         0.0f);

  const float cameraYaw90[16] = {0.0f, 0.0f, -1.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f,  0.0f,
                                 1.0f, 0.0f, 0.0f,  0.0f,
                                 0.0f, 0.0f, 0.0f,  1.0f};
  const auto yawedEyeLight =
      Viewer3DLightingProfile::TransformWorldDirectionToEyeSpace(
          Viewer3DLightingProfile::kKeyLightWorldDirection, cameraYaw90);
  AssertVectorNear(yawedEyeLight,
                   Viewer3DLightingProfile::NormalizeDirection(
                       {5.0f, -4.0f, -2.0f}));
  const auto yawedEyeFillLight =
      Viewer3DLightingProfile::TransformWorldDirectionToEyeSpace(
          Viewer3DLightingProfile::kFillLightWorldDirection, cameraYaw90);
  AssertVectorNear(yawedEyeFillLight,
                   Viewer3DLightingProfile::NormalizeDirection(
                       {1.0f, 2.0f, 1.5f}));

  const auto worldSurfaceNormal = worldLight;
  const auto yawedEyeNormal = TransformNormal(worldSurfaceNormal, cameraYaw90);
  assert(std::fabs(Dot(worldSurfaceNormal, worldLight) -
                   Dot(yawedEyeNormal, yawedEyeLight)) < 0.0001f);
  assert(Dot(yawedEyeNormal, yawedEyeLight) > 0.0f);
  Viewer3DLightingProfile::LightingState yawedLighting = worldLighting;
  yawedLighting.keyLightEyeDirection = yawedEyeLight;
  yawedLighting.fillLightEyeDirection = yawedEyeFillLight;
  assert(std::fabs(Viewer3DLightingProfile::CombinedDirectionalDiffuse(
                       worldSurfaceNormal, worldLighting) -
                   Viewer3DLightingProfile::CombinedDirectionalDiffuse(
                       yawedEyeNormal, yawedLighting)) < 0.0001f);
  assert(std::fabs(Viewer3DLightingProfile::CombinedDirectionalDiffuse(
                       yawedEyeNormal, yawedLighting) -
                   ShaderCombinedDiffuse(yawedEyeNormal, yawedLighting)) <
         0.0001f);

  const std::array<float, 3> objectNormal = {1.0f, 0.0f, 0.0f};
  const auto rotatedObjectNormal = TransformNormal(objectNormal, rotateZ90);
  AssertVectorNear(yawedEyeLight,
                   Viewer3DLightingProfile::TransformWorldDirectionToEyeSpace(
                       Viewer3DLightingProfile::kKeyLightWorldDirection,
                       cameraYaw90));
  assert(std::fabs(Dot(rotatedObjectNormal, worldLight) -
                   Dot(objectNormal, worldLight)) > 0.1f);

  const auto backNormal = Viewer3DSketchLighting::OrientNormalForFace(
      worldLight, false, true);
  assert(Viewer3DLightingProfile::CombinedDirectionalDiffuse(backNormal,
                                                              worldLighting) <
         Viewer3DLightingProfile::CombinedDirectionalDiffuse(worldLight,
                                                              worldLighting));

  const auto oldCameraFixedLight = Viewer3DLightingProfile::NormalizeDirection(
      {0.35f, -0.55f, 1.0f});
  assert(Dot(normal, oldCameraFixedLight) > 0.0f);
  assert(Viewer3DLightingProfile::CombinedDirectionalDiffuse(normal,
                                                              worldLighting) >
         0.0f);
  assert(Dot(worldFillLight, oldCameraFixedLight) <= 0.0f);
  assert(Viewer3DLightingProfile::CombinedDirectionalDiffuse(worldFillLight,
                                                              worldLighting) >
         0.0f);

  Mesh normalSourceMesh;
  normalSourceMesh.vertices = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                               0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};
  normalSourceMesh.indices = {0, 1, 2, 1, 3, 2};
  normalSourceMesh.normals = {0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f,
                              0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f};
  BuildFlatShadingBuffers(normalSourceMesh);
  const std::array<float, 3> geometricNormal =
      Viewer3DMeshShading::ComputeFaceNormal(
          {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
          {0.0f, 1.0f, 0.0f});
  const std::array<float, 3> oppositeVertexNormal = {0.0f, 0.0f, -1.0f};
  const auto flatMode = Viewer3DMeshShading::ResolveMode(true);
  const auto smoothMode = Viewer3DMeshShading::ResolveMode(false);
  const auto fixtureAndObjectMode = Viewer3DMeshShading::ResolveMode(true);
  const auto trussMode = Viewer3DMeshShading::ResolveMode(false);
  assert(flatMode == Viewer3DMeshShading::Mode::Flat);
  assert(smoothMode == Viewer3DMeshShading::Mode::Smooth);
  assert(fixtureAndObjectMode == Viewer3DMeshShading::Mode::Flat);
  assert(trussMode == Viewer3DMeshShading::Mode::Smooth);
  AssertVectorNear(Viewer3DMeshShading::SelectNormal(
                       flatMode, geometricNormal, oppositeVertexNormal, true),
                   geometricNormal);
  AssertVectorNear(Viewer3DMeshShading::SelectNormal(
                       smoothMode, geometricNormal, oppositeVertexNormal, true),
                   oppositeVertexNormal);
  AssertVectorNear(Viewer3DMeshShading::SelectNormal(
                       flatMode, geometricNormal, geometricNormal, true),
                   Viewer3DMeshShading::SelectNormal(
                       smoothMode, geometricNormal, geometricNormal, true));
  for (size_t i = 0; i < normalSourceMesh.flatNormals.size(); i += 3) {
    AssertVectorNear({normalSourceMesh.flatNormals[i],
                      normalSourceMesh.flatNormals[i + 1],
                      normalSourceMesh.flatNormals[i + 2]},
                     geometricNormal);
  }
  AssertVectorNear(Viewer3DMeshShading::SelectNormal(
                       smoothMode, geometricNormal,
                       {normalSourceMesh.normals[9],
                        normalSourceMesh.normals[10],
                        normalSourceMesh.normals[11]},
                       true),
                   geometricNormal);
  AssertVectorNear(
      TransformNormal(Viewer3DMeshShading::SelectNormal(
                          flatMode, geometricNormal, oppositeVertexNormal, true),
                      mirrored),
      geometricNormal);
  AssertVectorNear(
      TransformNormal(Viewer3DMeshShading::SelectNormal(
                          smoothMode, geometricNormal, oppositeVertexNormal,
                          true),
                      mirrored),
      oppositeVertexNormal);
  return 0;
}
