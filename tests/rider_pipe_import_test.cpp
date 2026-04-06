#include <cassert>
#include <cmath>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"

namespace {

constexpr float kEpsilon = 0.001f;

bool NearlyEqual(float a, float b) { return std::abs(a - b) < kEpsilon; }

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();

  cfg.Reset();
  const std::string pipeWithLengthText =
      "ILUMIN\n"
      "LX1\n"
      "2 BLINDER 2 PRO\n"
      "RIGGING\n"
      "1 PIPE 14m PARA LX1\n";
  assert(RiderImporter::ImportText(pipeWithLengthText));

  const auto &pipeWithLengthScene = cfg.GetScene();
  assert(pipeWithLengthScene.trusses.empty());
  assert(pipeWithLengthScene.sceneObjects.size() == 1);
  assert(pipeWithLengthScene.fixtures.size() == 2);

  bool foundPipeObject = false;
  for (const auto &[uuid, object] : pipeWithLengthScene.sceneObjects) {
    (void)uuid;
    foundPipeObject = true;
    assert(object.name.find("PIPE") == 0);
    assert(object.geometries.size() == 1);
    assert(object.geometries.front().modelFile == "primitive:cylinder");
    assert(NearlyEqual(object.transform.u[0], 14000.0f / 300.0f));
    assert(NearlyEqual(object.transform.v[1], 100.0f / 300.0f));
    assert(NearlyEqual(object.transform.w[2], 100.0f / 300.0f));
  }
  assert(foundPipeObject);

  for (const auto &[uuid, fixture] : pipeWithLengthScene.fixtures) {
    (void)uuid;
    assert(fixture.positionName == "LX1");
    assert(NearlyEqual(fixture.transform.o[1], -2000.0f));
    assert(NearlyEqual(fixture.transform.o[2], 10000.0f));
  }

  cfg.Reset();
  const std::string defaultPipeLengthText =
      "RIGGING\n"
      "3 VARAS PARA PUENTES LX\n";
  assert(RiderImporter::ImportText(defaultPipeLengthText));

  const auto &defaultPipeLengthScene = cfg.GetScene();
  assert(defaultPipeLengthScene.trusses.empty());
  assert(defaultPipeLengthScene.sceneObjects.size() == 3);

  int lx1Count = 0;
  int lx2Count = 0;
  int lx3Count = 0;
  for (const auto &[uuid, object] : defaultPipeLengthScene.sceneObjects) {
    (void)uuid;
    if (object.layer == "pos LX1")
      ++lx1Count;
    else if (object.layer == "pos LX2")
      ++lx2Count;
    else if (object.layer == "pos LX3")
      ++lx3Count;
    assert(object.geometries.size() == 1);
    assert(object.geometries.front().modelFile == "primitive:cylinder");
    assert(NearlyEqual(object.transform.u[0], 14000.0f / 300.0f));
  }
  assert(lx1Count == 1);
  assert(lx2Count == 1);
  assert(lx3Count == 1);

  cfg.Reset();
  const std::string pipeCoordinateOverrideText =
      "ILUMIN\n"
      "LX1\n"
      "1 BLINDER 2 PRO\n"
      "RIGGING\n"
      "1 PIPE 10m PARA LX1 (1, 3, 7)\n";
  assert(RiderImporter::ImportText(pipeCoordinateOverrideText));

  const auto &pipeCoordinateOverrideScene = cfg.GetScene();
  assert(pipeCoordinateOverrideScene.fixtures.size() == 1);
  const auto fixtureIt = pipeCoordinateOverrideScene.fixtures.begin();
  assert(NearlyEqual(fixtureIt->second.transform.o[1], 3000.0f));
  assert(NearlyEqual(fixtureIt->second.transform.o[2], 7000.0f));

  return 0;
}
