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
    assert(fixture.layer == "pos LX SIDES");
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
      "4 PAR\n"
      "RIGGING\n"
      "1 TRUSS 40X40 12m PARA LX1\n";
  assert(RiderImporter::ImportText(withoutSideTruss));
  const auto &sceneNoTruss = cfg.GetScene();

  int noTrussFixtureCount = 0;
  int lx3FixtureCount = 0;
  std::set<float> noTrussSideX;
  for (const auto &[uuid, fixture] : sceneNoTruss.fixtures) {
    (void)uuid;
    if (fixture.positionName != "LX SIDES")
      ++lx3FixtureCount;
    else {
      ++noTrussFixtureCount;
      assert(NearlyEqual(fixture.transform.o[2], 1000.0f));
      assert(fixture.layer == "pos SIDES");
      noTrussSideX.insert(fixture.transform.o[0]);
    }
  }
  assert(noTrussFixtureCount == 4);
  assert(lx3FixtureCount == 2);
  assert(noTrussSideX.size() == 2);
  assert(NearlyEqual(*noTrussSideX.begin(), -6500.0f));
  assert(NearlyEqual(*noTrussSideX.rbegin(), 6500.0f));

  cfg.Reset();
  const std::string alreadyFiltered =
      "LX3\n"
      "2 SPOT\n"
      "\n"
      "LX SIDES\n"
      "4 PAR\n";
  assert(RiderImporter::ImportText(alreadyFiltered));
  const auto &sceneFiltered = cfg.GetScene();

  int filteredSidesCount = 0;
  int filteredLx3Count = 0;
  for (const auto &[uuid, fixture] : sceneFiltered.fixtures) {
    (void)uuid;
    if (fixture.positionName == "LX SIDES") {
      ++filteredSidesCount;
      assert(fixture.layer == "pos SIDES");
      assert(NearlyEqual(fixture.transform.o[2], 1000.0f));
    } else if (fixture.positionName == "LX3") {
      ++filteredLx3Count;
    }
  }
  assert(filteredSidesCount == 4);
  assert(filteredLx3Count == 2);

  cfg.Reset();
  cfg.SetValue("ui_distance_unit_system", "metric");
  const std::string withCoordinateOverrideMetric =
      "RIGGING\n"
      "1 TRUSS LX1 (abc 0, -1, 9 z) 14m\n";
  assert(RiderImporter::ImportText(withCoordinateOverrideMetric));
  const auto &sceneMetric = cfg.GetScene();
  int metricTrussCount = 0;
  for (const auto &[uuid, truss] : sceneMetric.trusses) {
    (void)uuid;
    ++metricTrussCount;
    assert(truss.positionName == "LX1");
    assert(NearlyEqual(truss.transform.o[0], 0.0f));
    assert(NearlyEqual(truss.transform.o[1], -1000.0f));
    assert(NearlyEqual(truss.transform.o[2], 9000.0f));
  }
  assert(metricTrussCount > 0);

  cfg.Reset();
  cfg.SetValue("ui_distance_unit_system", "imperial");
  const std::string withCoordinateOverrideImperial =
      "RIGGING\n"
      "1 TRUSS LX1 (2, 10) 14m\n";
  assert(RiderImporter::ImportText(withCoordinateOverrideImperial));
  const auto &sceneImperial = cfg.GetScene();
  int imperialTrussCount = 0;
  for (const auto &[uuid, truss] : sceneImperial.trusses) {
    (void)uuid;
    ++imperialTrussCount;
    assert(truss.positionName == "LX1");
    // Two values map to Y/Z in active distance units (feet here).
    assert(NearlyEqual(truss.transform.o[1], 2.0f * 304.8f));
    assert(NearlyEqual(truss.transform.o[2], 10.0f * 304.8f));
  }
  assert(imperialTrussCount > 0);

  cfg.Reset();
  cfg.SetValue("ui_distance_unit_system", "metric");
  const std::string withSingleCoordinateOverride =
      "RIGGING\n"
      "1 TRUSS LX1 (7) 14m\n";
  assert(RiderImporter::ImportText(withSingleCoordinateOverride));
  const auto &sceneSingle = cfg.GetScene();
  int singleTrussCount = 0;
  for (const auto &[uuid, truss] : sceneSingle.trusses) {
    (void)uuid;
    ++singleTrussCount;
    assert(truss.positionName == "LX1");
    // One value maps only to Y; X/Z keep defaults for LX1.
    assert(NearlyEqual(truss.transform.o[1], 7000.0f));
    assert(NearlyEqual(truss.transform.o[2], 10000.0f));
  }
  assert(singleTrussCount > 0);

  cfg.Reset();
  cfg.SetValue("ui_distance_unit_system", "metric");
  const std::string headerCoordinateOverrideWithoutColon =
      "LX1 (6)\n"
      "2 SPOT\n"
      "RIGGING\n"
      "1 TRUSS 40X40 14m\n";
  assert(RiderImporter::ImportText(headerCoordinateOverrideWithoutColon));
  const auto &sceneHeaderOverride = cfg.GetScene();
  int headerFixtures = 0;
  int headerTrusses = 0;
  for (const auto &[uuid, fixture] : sceneHeaderOverride.fixtures) {
    (void)uuid;
    if (fixture.positionName == "LX1")
      ++headerFixtures;
  }
  for (const auto &[uuid, truss] : sceneHeaderOverride.trusses) {
    (void)uuid;
    if (truss.positionName != "LX1")
      continue;
    ++headerTrusses;
    assert(NearlyEqual(truss.transform.o[1], 6000.0f));
  }
  assert(headerFixtures == 2);
  assert(headerTrusses > 0);

  return 0;
}
