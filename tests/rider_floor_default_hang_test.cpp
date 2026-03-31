#include <cassert>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
#include "fixture.h"
#include "riderimporter.h"

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();

  const std::string riderText = "8 Spot\n"
                                "4 Wash\n";
  assert(RiderImporter::ImportText(riderText));

  const auto &scene = cfg.GetScene();
  assert(scene.fixtures.size() == 12);
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    assert(fixture.positionName == "FLOOR");
    assert(fixture.layer == "pos FLOOR");
  }

  const std::string preview = RiderImporter::BuildFixtureFilterPreview(riderText);
  const std::string expectedPreview = "FLOOR\n"
                                      "8 Spot\n"
                                      "4 Wash";
  assert(preview == expectedPreview);

  return 0;
}
