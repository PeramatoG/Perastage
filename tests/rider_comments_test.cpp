#include <cassert>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();

  const std::string riderText =
      "*(Example with inline annotations)*\n"
      "LX1 *(Main front truss)*\n"
      "4 Spot *(Main spots)*\n"
      "RIGGING\n"
      "1 TRUSS 40X40 12m PARA LX1 *(Truss definition)*\n";

  const std::string expectedPreview =
      "LX1\n"
      "4 Spot\n"
      "\n"
      "\n"
      "RIGGING\n"
      "1 TRUSS 40X40 12m LX1";
  const std::string preview = RiderImporter::BuildFixtureFilterPreview(riderText);
  assert(preview == expectedPreview);

  assert(RiderImporter::ImportText(riderText));
  const auto &scene = cfg.GetScene();
  assert(scene.fixtures.size() == 4);
  assert(scene.trusses.size() == 4);

  return 0;
}
