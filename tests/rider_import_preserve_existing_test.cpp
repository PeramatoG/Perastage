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

  Fixture existing;
  existing.uuid = "existing-fixture";
  existing.typeName = "ExistingType";
  existing.instanceName = "ExistingType 7";
  existing.positionName = "LX1";
  existing.fixtureId = 507;
  existing.unitNumber = 7;
  existing.transform.o[0] = 1234.0f;
  existing.transform.o[1] = 5678.0f;
  existing.transform.o[2] = 910.0f;
  cfg.GetScene().fixtures.emplace(existing.uuid, existing);

  const std::string riderText = "ilumin\n"
                                "LX1\n"
                                "2 Spot\n";
  assert(RiderImporter::ImportText(riderText));

  const auto &scene = cfg.GetScene();
  auto existingIt = scene.fixtures.find(existing.uuid);
  assert(existingIt != scene.fixtures.end());

  const Fixture &preserved = existingIt->second;
  assert(preserved.fixtureId == 507);
  assert(preserved.unitNumber == 7);
  assert(preserved.instanceName == "ExistingType 7");
  assert(preserved.transform.o[0] == 1234.0f);
  assert(preserved.transform.o[1] == 5678.0f);
  assert(preserved.transform.o[2] == 910.0f);

  int importedCount = 0;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (uuid == existing.uuid)
      continue;
    ++importedCount;
    assert(fixture.fixtureId >= 601);
  }
  assert(importedCount == 2);

  return 0;
}
