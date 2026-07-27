#include <cassert>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"

// Verifies that Rider annotations do not change preview blocks or imports.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();

  const std::string riderText =
      "*(Example with inline annotations)*\n"
      "*(A second leading comment)*\n"
      "LX1 *(Main front truss)*\n"
      "4 Spot *(Main spots)*\n"
      "*(Comment between non-empty blocks)*\n"
      "*(Consecutive comment between blocks)*\n"
      "RIGGING\n"
      "1 TRUSS 40X40 12m PARA LX1 *(Truss definition)*\n";

  const std::string expectedPreview =
      "LX1\n"
      "4 Spot\n"
      "\n"
      "RIGGING\n"
      "1 TRUSS 40X40 12m LX1";
  const std::string preview = RiderImporter::BuildFixtureFilterPreview(riderText);
  assert(preview == expectedPreview);

  assert(RiderImporter::ImportText(riderText));
  const auto &scene = cfg.GetScene();
  assert(scene.fixtures.size() == 4);
  assert(scene.trusses.size() == 4);
  const auto directFixtureCount = scene.fixtures.size();
  const auto directTrussCount = scene.trusses.size();
  const auto directSupportCount = scene.supports.size();
  const auto directSceneObjectCount = scene.sceneObjects.size();

  cfg.Reset();
  assert(RiderImporter::ImportText(preview));
  const auto &filteredScene = cfg.GetScene();
  assert(filteredScene.fixtures.size() == directFixtureCount);
  assert(filteredScene.trusses.size() == directTrussCount);
  assert(filteredScene.supports.size() == directSupportCount);
  assert(filteredScene.sceneObjects.size() == directSceneObjectCount);

  return 0;
}
