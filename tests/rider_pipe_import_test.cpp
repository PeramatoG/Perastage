#include <cassert>
#include <cmath>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"

namespace {

constexpr float kEpsilon = 0.001f;

// Compares scene dimensions within the importer test tolerance.
bool NearlyEqual(float a, float b) { return std::abs(a - b) < kEpsilon; }

} // namespace

// Verifies pipe target expansion and primitive scene-object creation.
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
    assert(object.name == "PIPE LX1");
    assert(object.geometries.size() == 1);
    assert(object.geometries.front().modelFile == "primitive:cylinder");
    assert(NearlyEqual(object.geometries.front().localTransform.u[0], 50.0f / 1000.0f));
    assert(NearlyEqual(object.geometries.front().localTransform.v[1], 50.0f / 1000.0f));
    assert(NearlyEqual(object.geometries.front().localTransform.w[2], 14000.0f / 1000.0f));
    assert(NearlyEqual(object.transform.u[0], 0.0f));
    assert(NearlyEqual(object.transform.u[2], -1.0f));
    assert(NearlyEqual(object.transform.v[1], 1.0f));
    assert(NearlyEqual(object.transform.w[0], 1.0f));
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
    assert(object.name == ("PIPE " + object.layer.substr(4)));
    assert(object.geometries.size() == 1);
    assert(object.geometries.front().modelFile == "primitive:cylinder");
    assert(NearlyEqual(object.geometries.front().localTransform.u[0], 50.0f / 1000.0f));
    assert(NearlyEqual(object.geometries.front().localTransform.w[2], 14000.0f / 1000.0f));
    assert(NearlyEqual(object.transform.u[0], 0.0f));
    assert(NearlyEqual(object.transform.u[2], -1.0f));
    assert(NearlyEqual(object.transform.w[0], 1.0f));
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


  const std::string expectedFilteredPipes =
      "RIGGING\n1 PIPE 14m LX1\n1 PIPE 14m LX2\n1 PIPE 14m LX3";
  assert(RiderImporter::BuildFixtureFilterPreview(defaultPipeLengthText) ==
         expectedFilteredPipes);
  assert(RiderImporter::BuildFixtureFilterPreview(expectedFilteredPipes) ==
         expectedFilteredPipes);

  const std::string repeatedExplicitTargetText =
      "RIGGING\n3 PIPE PARA LX1\n";
  const std::string filteredExplicitTarget =
      RiderImporter::BuildFixtureFilterPreview(repeatedExplicitTargetText);
  assert(filteredExplicitTarget ==
         "RIGGING\n1 PIPE 14m LX1\n1 PIPE 14m LX1\n1 PIPE 14m LX1");
  for (const std::string &input :
       {repeatedExplicitTargetText, filteredExplicitTarget}) {
    cfg.Reset();
    assert(RiderImporter::ImportText(input));
    const auto &scene = cfg.GetScene();
    assert(scene.sceneObjects.size() == 3);
    for (const auto &[uuid, object] : scene.sceneObjects) {
      (void)uuid;
      assert(object.name == "PIPE LX1");
      assert(object.layer == "pos LX1");
      assert(object.geometries.size() == 1);
      assert(object.geometries.front().modelFile == "primitive:cylinder");
      assert(NearlyEqual(object.geometries.front().localTransform.w[2], 14.0f));
    }
  }

  return 0;
}
