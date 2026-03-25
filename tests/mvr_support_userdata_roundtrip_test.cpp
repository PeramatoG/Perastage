/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <wx/init.h>

#include "configmanager.h"
#include "fixture.h"
#include "layer.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "projectutils.h"
#include "support.h"

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();

  Layer layer;
  layer.uuid = "layer1";
  layer.name = "Layer1";
  scene.layers[layer.uuid] = layer;

  std::filesystem::path tempDir =
      std::filesystem::temp_directory_path() / "mvr_support_userdata_roundtrip";
  std::filesystem::create_directories(tempDir);
  std::ofstream(tempDir / "fixture.gdtf") << "fixture";
  scene.basePath = tempDir.string();

  Fixture motorFixture;
  motorFixture.uuid = "fx-motor";
  motorFixture.instanceName = "Motor Fixture";
  motorFixture.layer = layer.name;
  motorFixture.typeName = "MotorType";
  motorFixture.gdtfSpec = "fixture.gdtf";
  motorFixture.gdtfMode = "ChainMode";
  motorFixture.category = "Spot";
  motorFixture.categorySource = "Manual";
  scene.fixtures[motorFixture.uuid] = motorFixture;

  Support linked;
  linked.uuid = "sup-linked";
  linked.name = "Linked";
  linked.layer = layer.name;
  linked.motorFixtureUuid = motorFixture.uuid;
  linked.motorName = "CM Lodestar";
  linked.useMotorDefaults = false;
  linked.hoistDataSource = "Inherited";
  scene.supports[linked.uuid] = linked;

  Support dummy;
  dummy.uuid = "sup-dummy";
  dummy.name = "Dummy";
  dummy.layer = layer.name;
  dummy.dummyProfileId = "d8plus_1000kg";
  dummy.hoistFunction = "Video";
  scene.supports[dummy.uuid] = dummy;

  Support inheritedDefaults;
  inheritedDefaults.uuid = "sup-inherited-defaults";
  inheritedDefaults.name = "Inherited Defaults";
  inheritedDefaults.layer = layer.name;
  inheritedDefaults.motorFixtureUuid = motorFixture.uuid;
  inheritedDefaults.dummyProfileId = "d8plus_1000kg";
  inheritedDefaults.hoistDataSource = "Inherited";
  inheritedDefaults.useMotorDefaults = true;
  HoistPresetDefaults inheritedPreset;
  inheritedPreset.motorManufacturer = "Perastage";
  inheritedPreset.capacityKg = 1000.0f;
  inheritedPreset.weightKg = 40.0f;
  inheritedPreset.hoistFunction = "Lighting";

  const auto effectiveInherited = ResolveEffectiveSupportData(
      inheritedDefaults, inheritedPreset, BuildHoistFixtureDefaults(motorFixture));
  inheritedDefaults.motorManufacturer = effectiveInherited.motorManufacturer;
  inheritedDefaults.motorModel = effectiveInherited.motorModel;
  scene.supports[inheritedDefaults.uuid] = inheritedDefaults;

  Support manual;
  manual.uuid = "sup-manual";
  manual.name = "Manual";
  manual.layer = layer.name;
  manual.hoistDataSource = "Manual";
  manual.capacityKg = 1250.0f;
  manual.weightKg = 61.0f;
  manual.loadKg = 410.0f;
  manual.hoistFunction = "Audio";
  manual.motorName = "ChainMaster D8+";
  manual.motorNameSource = "Manual";
  manual.motorManufacturer = "ChainMaster";
  manual.motorManufacturerSource = "Manual";
  manual.motorModel = "D8+";
  manual.motorModelSource = "Manual";
  manual.capacitySource = "Manual";
  manual.weightSource = "Manual";
  manual.hoistFunctionSource = "Manual";
  scene.supports[manual.uuid] = manual;

  const std::filesystem::path mvrPath = tempDir / "support_roundtrip.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(mvrPath.string()));

  cfg.Reset();
  MvrImporter importer;
  assert(importer.ImportFromFile(mvrPath.string(), false, false));

  const auto &loadedFixtures = cfg.GetScene().fixtures;
  assert(loadedFixtures.size() == 1);
  assert(loadedFixtures.at("fx-motor").category == "Spot");

  const auto &loaded = cfg.GetScene().supports;
  assert(loaded.size() == 4);

  const auto &loadedLinked = loaded.at("sup-linked");
  assert(loadedLinked.motorFixtureUuid == "fx-motor");
  assert(!loadedLinked.useMotorDefaults);

  const auto &loadedDummy = loaded.at("sup-dummy");
  assert(loadedDummy.dummyProfileId == "d8plus_1000kg");
  assert(loadedDummy.hoistFunction == "Video");

  const auto &loadedManual = loaded.at("sup-manual");
  assert(loadedManual.hoistDataSource == "Manual");
  assert(loadedManual.capacityKg == 1250.0f);
  assert(loadedManual.weightKg == 61.0f);
  assert(loadedManual.loadKg == 410.0f);
  assert(loadedManual.hoistFunction == "Audio");
  assert(loadedManual.motorName == "ChainMaster D8+");
  assert(loadedManual.motorManufacturer == "ChainMaster");
  assert(loadedManual.motorModel == "D8+");
  assert(loadedManual.motorNameSource == "Manual");
  assert(loadedManual.motorManufacturerSource == "Manual");
  assert(loadedManual.motorModelSource == "Manual");
  assert(loadedManual.capacitySource == "Manual");
  assert(loadedManual.weightSource == "Manual");
  assert(loadedManual.hoistFunctionSource == "Manual");


  const auto &loadedInheritedDefaults = loaded.at("sup-inherited-defaults");
  assert(loadedInheritedDefaults.hoistDataSource == "Inherited");
  assert(loadedInheritedDefaults.useMotorDefaults);
  assert(loadedInheritedDefaults.motorManufacturer == "Perastage");
  assert(loadedInheritedDefaults.motorModel == "ChainMode");
  std::filesystem::remove_all(tempDir);
  std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") + "/fixture.gdtf");
  return 0;
}
