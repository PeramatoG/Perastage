#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
#include <wx/init.h>

#include "configmanager.h"
#include "fixture.h"
#include "riderimporter.h"

int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  assert(argc >= 2);

  auto &cfg = ConfigManager::Get();
  cfg.Reset();

  RiderImporter importer;
  assert(importer.Import(argv[1]));

  const auto &scene = cfg.GetScene();

  std::vector<const Fixture *> lx1Fixtures;
  std::vector<const Fixture *> lx2Fixtures;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    if (fixture.positionName == "LX1")
      lx1Fixtures.push_back(&fixture);
    if (fixture.positionName == "LX2")
      lx2Fixtures.push_back(&fixture);
  }

  assert(lx1Fixtures.size() == 8);
  assert(lx2Fixtures.size() == 1);

  std::sort(lx1Fixtures.begin(), lx1Fixtures.end(),
            [](const Fixture *a, const Fixture *b) {
              return a->transform.o[0] < b->transform.o[0];
            });

  const std::vector<std::string> expectedTypes = {
      "MegaPointe", "Spiider", "Spiider", "MegaPointe",
      "MegaPointe", "Spiider", "Spiider", "MegaPointe"};
  for (size_t i = 0; i < expectedTypes.size(); ++i)
    assert(lx1Fixtures[i]->typeName == expectedTypes[i]);

  const std::vector<float> expectedYOffsets = {-200.0f, -200.0f, -200.0f, -200.0f,
                                               -200.0f, -200.0f, -200.0f, -200.0f};
  for (size_t i = 0; i < expectedYOffsets.size(); ++i)
    assert(std::abs(lx1Fixtures[i]->transform.o[1] - expectedYOffsets[i]) < 1e-3f);

  const Fixture *single = lx2Fixtures.front();
  assert(std::abs(single->transform.o[0]) < 1e-3f);

  return 0;
}
