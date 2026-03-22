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
                                "2 Spot\n";
  assert(RiderImporter::ImportText(riderText));

  const auto &fixtures = cfg.GetScene().fixtures;
  assert(fixtures.size() == 2);
  for (const auto &[uuid, fixture] : fixtures) {
    (void)uuid;
    assert(fixture.category == GdtfFixtureCategory::kUnknown);
    assert(fixture.categorySource == GdtfFixtureCategory::kAutoFallbackSource);
  }

  return 0;
}
