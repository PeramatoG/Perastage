#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <string>
#include <vector>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"

namespace {
bool NearlyEqual(float a, float b) {
  return std::abs(a - b) < 0.001f;
}
}

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();

  cfg.Reset();
  const std::string withSideTruss =
      "ILUMINACION\n"
      "LX1:\n"
      "4 SPOT\n"
      "SIDES:\n"
      "6 WASH\n"
      "RIGGING\n"
      "1 TRUSS 40X40 10m PARA LX1\n"
      "1 TRUSS 40X40 6m PARA SIDES\n";

  assert(RiderImporter::ImportText(withSideTruss));
  const auto &sceneWithTruss = cfg.GetScene();

  int sideTrussCount = 0;
  std::set<float> sideTrussX;
  for (const auto &[uuid, truss] : sceneWithTruss.trusses) {
    (void)uuid;
    if (truss.positionName != "LX SIDES")
      continue;
    ++sideTrussCount;
    sideTrussX.insert(truss.transform.o[0]);
    assert(NearlyEqual(truss.transform.o[2], 5000.0f));
    assert(NearlyEqual(truss.transform.u[0], 0.0f));
    assert(NearlyEqual(truss.transform.u[1], 1.0f));
  }
  assert(sideTrussCount >= 2);
  assert(sideTrussX.size() >= 2);

  std::vector<float> fixtureX;
  std::vector<float> fixtureY;
  for (const auto &[uuid, fixture] : sceneWithTruss.fixtures) {
    (void)uuid;
    if (fixture.positionName != "LX SIDES")
      continue;
    fixtureX.push_back(fixture.transform.o[0]);
    fixtureY.push_back(fixture.transform.o[1]);
    assert(NearlyEqual(fixture.transform.o[2], 5000.0f));
  }
  assert(fixtureX.size() == 6);
  std::sort(fixtureX.begin(), fixtureX.end());
  std::sort(fixtureY.begin(), fixtureY.end());
  assert(fixtureX.front() < 0.0f);
  assert(fixtureX.back() > 0.0f);
  assert(!NearlyEqual(fixtureY.front(), fixtureY.back()));

  cfg.Reset();
  const std::string withoutSideTruss =
      "ILUMINACION\n"
      "LX3:\n"
      "2 SPOT\n"
      "CALLES EN LAYHER:\n"
      "4 PAR\n";
  assert(RiderImporter::ImportText(withoutSideTruss));
  const auto &sceneNoTruss = cfg.GetScene();

  int noTrussFixtureCount = 0;
  int lx3FixtureCount = 0;
  for (const auto &[uuid, fixture] : sceneNoTruss.fixtures) {
    (void)uuid;
    if (fixture.positionName != "LX SIDES")
      ++lx3FixtureCount;
    else {
      ++noTrussFixtureCount;
      assert(NearlyEqual(fixture.transform.o[2], 1000.0f));
    }
  }
  assert(noTrussFixtureCount == 4);
  assert(lx3FixtureCount == 2);

  return 0;
}
