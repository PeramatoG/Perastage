#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
#include "primitive_transform.h"
#include "riderimporter.h"
#include "sceneobject.h"

namespace {
constexpr float kDimensionToleranceMeters = 1e-3f;
constexpr float kPositionToleranceMillimeters = 1.0f;
constexpr float kScreenThicknessMeters = 0.1f;
constexpr float kScreenTopGapMillimeters = 200.0f;

struct ScreenSnapshot {
  std::string name;
  std::string layer;
  std::string primitiveToken;
  Matrix objectTransform;
  Matrix geometryTransform;
  float trussCenterX = 0.0f;
  float trussZ = 0.0f;
};

// Compares scalar values within a named tolerance.
bool NearlyEqual(float left, float right, float tolerance) {
  return std::fabs(left - right) < tolerance;
}

// Imports Rider text and captures the screen's semantic geometry and placement.
ScreenSnapshot ImportAndCaptureScreen(const std::string &riderText) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  assert(RiderImporter::ImportText(riderText));

  const auto &scene = cfg.GetScene();
  assert(scene.fixtures.empty());
  assert(scene.sceneObjects.size() == 1);

  const SceneObject &screen = scene.sceneObjects.begin()->second;
  assert(screen.geometries.size() == 1);

  float trussStartX = 0.0f;
  float trussEndX = 0.0f;
  float trussZ = 0.0f;
  bool foundScreenTruss = false;
  for (const auto &[uuid, truss] : scene.trusses) {
    (void)uuid;
    if (truss.positionName != "SCREEN")
      continue;
    const float startX = truss.transform.o[0];
    const float endX = startX + truss.transform.u[0] * truss.lengthMm;
    if (!foundScreenTruss) {
      trussStartX = std::min(startX, endX);
      trussEndX = std::max(startX, endX);
      trussZ = truss.transform.o[2];
      foundScreenTruss = true;
    } else {
      trussStartX = std::min(trussStartX, std::min(startX, endX));
      trussEndX = std::max(trussEndX, std::max(startX, endX));
    }
  }
  assert(foundScreenTruss);

  return {screen.name,
          screen.layer,
          screen.geometries.front().modelFile,
          screen.transform,
          screen.geometries.front().localTransform,
          (trussStartX + trussEndX) * 0.5f,
          trussZ};
}

// Verifies one screen snapshot against canonical cube dimensions and placement.
void AssertScreenContract(const ScreenSnapshot &screen,
                          float expectedWidthMeters,
                          float expectedHeightMeters) {
  assert(screen.primitiveToken == "primitive:cube");
  assert(NearlyEqual(PrimitiveTransform::kCanonicalCubeSizeMeters, 1.0f,
                     kDimensionToleranceMeters));
  assert(NearlyEqual(screen.geometryTransform.u[0], expectedWidthMeters,
                     kDimensionToleranceMeters));
  assert(NearlyEqual(screen.geometryTransform.v[1], kScreenThicknessMeters,
                     kDimensionToleranceMeters));
  assert(NearlyEqual(screen.geometryTransform.w[2], expectedHeightMeters,
                     kDimensionToleranceMeters));

  const float composedWidth = PrimitiveTransform::kCanonicalCubeSizeMeters *
                              std::fabs(screen.geometryTransform.u[0]);
  const float composedThickness = PrimitiveTransform::kCanonicalCubeSizeMeters *
                                  std::fabs(screen.geometryTransform.v[1]);
  const float composedHeight = PrimitiveTransform::kCanonicalCubeSizeMeters *
                               std::fabs(screen.geometryTransform.w[2]);
  assert(NearlyEqual(composedWidth, expectedWidthMeters,
                     kDimensionToleranceMeters));
  assert(NearlyEqual(composedThickness, kScreenThicknessMeters,
                     kDimensionToleranceMeters));
  assert(NearlyEqual(composedHeight, expectedHeightMeters,
                     kDimensionToleranceMeters));

  assert(NearlyEqual(screen.objectTransform.u[0], 1.0f,
                     kDimensionToleranceMeters));
  assert(NearlyEqual(screen.objectTransform.v[1], 1.0f,
                     kDimensionToleranceMeters));
  assert(NearlyEqual(screen.objectTransform.w[2], 1.0f,
                     kDimensionToleranceMeters));
  assert(NearlyEqual(screen.objectTransform.o[0], screen.trussCenterX,
                     kPositionToleranceMillimeters));
  const float screenTopZ =
      screen.objectTransform.o[2] + expectedHeightMeters * 500.0f;
  assert(NearlyEqual(screenTopZ, screen.trussZ - kScreenTopGapMillimeters,
                     kPositionToleranceMillimeters));
}

// Verifies equivalent screen snapshots without comparing generated UUIDs.
void AssertEquivalent(const ScreenSnapshot &left, const ScreenSnapshot &right) {
  assert(left.name == right.name);
  assert(left.layer == right.layer);
  assert(left.primitiveToken == right.primitiveToken);
  assert(NearlyEqual(left.objectTransform.o[0], right.objectTransform.o[0],
                     kPositionToleranceMillimeters));
  assert(NearlyEqual(left.objectTransform.o[1], right.objectTransform.o[1],
                     kPositionToleranceMillimeters));
  assert(NearlyEqual(left.objectTransform.o[2], right.objectTransform.o[2],
                     kPositionToleranceMillimeters));
  assert(NearlyEqual(left.geometryTransform.u[0], right.geometryTransform.u[0],
                     kDimensionToleranceMeters));
  assert(NearlyEqual(left.geometryTransform.v[1], right.geometryTransform.v[1],
                     kDimensionToleranceMeters));
  assert(NearlyEqual(left.geometryTransform.w[2], right.geometryTransform.w[2],
                     kDimensionToleranceMeters));
}
} // namespace

// Verifies Rider screens use canonical cube transforms and stable placement.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const std::string riderText =
      "ILUMINACION\nSCREEN\n"
      "1 PANTALLA LED 8X5m 1664X1040 PIXELS Y 1100Kg PARA TRASERA\n"
      "RIGGING\n1 TRUSS 40X40 14m PARA PANTALLA\n";
  const ScreenSnapshot direct = ImportAndCaptureScreen(riderText);
  AssertScreenContract(direct, 8.0f, 5.0f);
  assert(!direct.name.empty());
  assert(!direct.layer.empty());

  const std::string filtered =
      RiderImporter::BuildFixtureFilterPreview(riderText);
  assert(!filtered.empty());
  const ScreenSnapshot filteredScreen = ImportAndCaptureScreen(filtered);
  AssertScreenContract(filteredScreen, 8.0f, 5.0f);
  AssertEquivalent(direct, filteredScreen);

  const ScreenSnapshot projection = ImportAndCaptureScreen(
      "VIDEO\nPROYECCION\n"
      "1 PANTALLA LED 8X4.5m 1664X936 PIXELS PARA TRASERA\n"
      "RIGGING\n1 TRUSS 40X40 PRO 14m PARA PUENTE PANTALLA\n");
  AssertScreenContract(projection, 8.0f, 4.5f);

  const ScreenSnapshot englishScreen = ImportAndCaptureScreen(
      "VIDEO\nLED SCREEN\n1 LED SCREEN 10X5m 1664x832 PIXELS\n"
      "RIGGING\n1 TRUSS 40X40 12m FOR SCREEN\n");
  AssertScreenContract(englishScreen, 10.0f, 5.0f);

  const ScreenSnapshot englishWall = ImportAndCaptureScreen(
      "VIDEO\nLED WALL\n1 LED WALL 10 x 5 m\n"
      "RIGGING\n1 TRUSS 40X40 12m FOR SCREEN\n");
  AssertScreenContract(englishWall, 10.0f, 5.0f);

  const ScreenSnapshot fallback =
      ImportAndCaptureScreen("SCREEN\n1 PANTALLA LED PARA TRASERA\n"
                             "RIGGING\n1 TRUSS 40X40 14m PARA PANTALLA\n");
  AssertScreenContract(fallback, 8.0f, 5.0f);

  return 0;
}
