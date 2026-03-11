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

  Support manual;
  manual.uuid = "sup-manual";
  manual.name = "Manual";
  manual.layer = layer.name;
  manual.hoistDataSource = "Manual";
  manual.capacityKg = 1250.0f;
  manual.weightKg = 61.0f;
  manual.hoistFunction = "Audio";
  manual.motorName = "ChainMaster D8+";
  scene.supports[manual.uuid] = manual;

  const std::filesystem::path mvrPath = tempDir / "support_roundtrip.mvr";
  assert(cfg.ExportMVR(mvrPath.string()));

  cfg.Reset();
  assert(cfg.ImportMVR(mvrPath.string(), false, false));

  const auto &loaded = cfg.GetScene().supports;
  assert(loaded.size() == 3);

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
  assert(loadedManual.hoistFunction == "Audio");

  std::filesystem::remove_all(tempDir);
  std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") + "/fixture.gdtf");
  return 0;
}
