#include <cassert>

#include "fixture.h"
#include "support.h"

int main() {
  Support support;
  support.hoistDataSource = "Inherited";
  support.useMotorDefaults = true;
  support.hoistFunction = "Lighting";

  HoistPresetDefaults preset;
  preset.motorName = "Preset Motor";
  preset.motorManufacturer = "Preset Manufacturer";
  preset.motorModel = "Preset Model";
  preset.capacityKg = 1000.0f;
  preset.weightKg = 40.0f;
  preset.hoistFunction = "Audio";

  Fixture fixture;
  fixture.typeName = "Fixture Motor";
  fixture.gdtfMode = "Mode A";
  fixture.weightKg = 55.0f;
  fixture.function = "Video";
  auto fixtureDefaults = BuildHoistFixtureDefaults(fixture);

  const auto inherited =
      ResolveEffectiveSupportData(support, preset, fixtureDefaults);
  assert(inherited.motorName == "Fixture Motor");
  assert(inherited.motorManufacturer == "Preset Manufacturer");
  assert(inherited.motorModel == "Mode A");
  assert(inherited.capacityKg == 1000.0f);
  assert(inherited.weightKg == 55.0f);
  assert(inherited.hoistFunction == "Video");

  support.capacityKg = 750.0f;
  support.motorName = "Manual Motor";
  support.motorManufacturer = "Manual Manufacturer";
  const auto manualPriority =
      ResolveEffectiveSupportData(support, preset, fixtureDefaults);
  assert(manualPriority.motorName == "Manual Motor");
  assert(manualPriority.motorManufacturer == "Manual Manufacturer");
  assert(manualPriority.capacityKg == 750.0f);

  support.useMotorDefaults = false;
  support.motorName.clear();
  support.motorManufacturer.clear();
  support.motorModel.clear();
  support.capacityKg = 0.0f;
  const auto noDefaults = ResolveEffectiveSupportData(support, preset, fixtureDefaults);
  assert(noDefaults.motorName.empty());
  assert(noDefaults.motorManufacturer.empty());
  assert(noDefaults.motorModel.empty());
  assert(noDefaults.capacityKg == 0.0f);

  support.hoistDataSource = "Manual";
  const auto manualSource = ResolveEffectiveSupportData(support, preset, fixtureDefaults);
  assert(manualSource.dataSource == "Manual");
  assert(manualSource.capacityKg == 0.0f);

  return 0;
}
