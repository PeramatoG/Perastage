#include <cassert>
#include <cmath>
#include <string>
#include <vector>
#include <wx/init.h>

#include "configmanager.h"
#include "fixture.h"
#include "gdtf_fixture_category.h"
#include "riderimporter.h"

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();

  const std::string riderText = "ilumin\n"
                                "LX1\n"
                                "2 Spot\n"
                                "1 Wash\n"
                                "1 Blinder\n"
                                "1 Estrobo\n"
                                "1 Turbina FX\n"
                                "1 Hybrid 300\n";
  assert(RiderImporter::ImportText(riderText));

  const auto &fixtures = cfg.GetScene().fixtures;
  assert(fixtures.size() == 7);

  std::vector<const Fixture *> spots;
  std::vector<const Fixture *> washes;
  std::vector<const Fixture *> blinders;
  std::vector<const Fixture *> strobes;
  int spotCount = 0;
  int washCount = 0;
  int blinderCount = 0;
  int strobeCount = 0;
  int smokeCount = 0;
  int hybridCount = 0;
  for (const auto &[uuid, fixture] : fixtures) {
    (void)uuid;
    if (fixture.typeName == "Spot") {
      ++spotCount;
      spots.push_back(&fixture);
      assert(fixture.category == GdtfFixtureCategory::kSpot);
    } else if (fixture.typeName == "Wash") {
      ++washCount;
      washes.push_back(&fixture);
      assert(fixture.category == GdtfFixtureCategory::kWash);
    } else if (fixture.typeName == "Blinder") {
      ++blinderCount;
      blinders.push_back(&fixture);
      assert(fixture.category == GdtfFixtureCategory::kBlinder);
    } else if (fixture.typeName == "Estrobo") {
      ++strobeCount;
      strobes.push_back(&fixture);
      assert(fixture.category == GdtfFixtureCategory::kStrobe);
    } else if (fixture.typeName == "Turbina FX") {
      ++smokeCount;
      assert(fixture.category == GdtfFixtureCategory::kSmoke);
    } else if (fixture.typeName == "Hybrid 300") {
      ++hybridCount;
      assert(fixture.category == GdtfFixtureCategory::kHybrid);
    } else {
      assert(false && "Unexpected fixture type in rider import test");
    }
    assert(fixture.categorySource == GdtfFixtureCategory::kAutoFallbackSource);
  }
  assert(spotCount == 2);
  assert(washCount == 1);
  assert(blinderCount == 1);
  assert(strobeCount == 1);
  assert(smokeCount == 1);
  assert(hybridCount == 1);
  assert(!spots.empty());
  assert(!washes.empty());
  assert(!blinders.empty());
  assert(!strobes.empty());

  const float kTolerance = 0.001f;
  const float frontBottomY = spots.front()->transform.o[1];
  const float bottomZ = spots.front()->transform.o[2];
  assert(std::fabs(washes.front()->transform.o[1] - (frontBottomY + 400.0f)) <
         kTolerance);
  assert(std::fabs(washes.front()->transform.o[2] - bottomZ) < kTolerance);
  assert(std::fabs(blinders.front()->transform.o[1] - frontBottomY) < kTolerance);
  assert(std::fabs(strobes.front()->transform.o[1] - frontBottomY) < kTolerance);
  assert(std::fabs(blinders.front()->transform.o[2] - (bottomZ + 200.0f)) <
         kTolerance);
  assert(std::fabs(strobes.front()->transform.o[2] - (bottomZ + 200.0f)) <
         kTolerance);

  return 0;
}
