#include <cassert>
#include <string>
#include <wx/init.h>

#include "configmanager.h"
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
                                "1 Blinder\n"
                                "1 Turbina FX\n";
  assert(RiderImporter::ImportText(riderText));

  const auto &fixtures = cfg.GetScene().fixtures;
  assert(fixtures.size() == 4);

  int spotCount = 0;
  int blinderCount = 0;
  int smokeCount = 0;
  for (const auto &[uuid, fixture] : fixtures) {
    (void)uuid;
    if (fixture.typeName == "Spot") {
      ++spotCount;
      assert(fixture.category == GdtfFixtureCategory::kSpot);
    } else if (fixture.typeName == "Blinder") {
      ++blinderCount;
      assert(fixture.category == GdtfFixtureCategory::kBlinder);
    } else if (fixture.typeName == "Turbina FX") {
      ++smokeCount;
      assert(fixture.category == GdtfFixtureCategory::kSmoke);
    } else {
      assert(false && "Unexpected fixture type in rider import test");
    }
    assert(fixture.categorySource == GdtfFixtureCategory::kAutoFallbackSource);
  }
  assert(spotCount == 2);
  assert(blinderCount == 1);
  assert(smokeCount == 1);

  return 0;
}
