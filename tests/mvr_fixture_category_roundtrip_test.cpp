/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <fstream>

#include <wx/init.h>

#include "configmanager.h"
#include "fixture.h"
#include "gdtfdictionary.h"
#include "layer.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "projectutils.h"

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

  const std::filesystem::path tempDir =
      std::filesystem::temp_directory_path() / "mvr_fixture_category_roundtrip";
  std::filesystem::remove_all(tempDir);
  std::filesystem::create_directories(tempDir);

  const std::filesystem::path fixturePath = tempDir / "fixture.gdtf";
  std::ofstream(fixturePath) << "fixture";
  scene.basePath = tempDir.string();

  Fixture fixture;
  fixture.uuid = "fx1";
  fixture.instanceName = "Fixture One";
  fixture.layer = layer.name;
  fixture.typeName = "FixtureType";
  fixture.gdtfSpec = fixturePath.string();
  fixture.gdtfMode = "ModeA";
  fixture.category = "Spot";
  fixture.categorySource = "Manual";
  scene.fixtures[fixture.uuid] = fixture;

  GdtfDictionary::Update("FixtureType", fixturePath.string(), "ModeA", "Wash");

  const std::filesystem::path mvrPath = tempDir / "fixture_category.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(mvrPath.string()));

  cfg.Reset();
  MvrImporter importer;
  assert(importer.ImportFromFile(mvrPath.string(), false, false));

  const auto &loaded = cfg.GetScene().fixtures;
  assert(loaded.size() == 1);
  const Fixture &loadedFixture = loaded.at("fx1");
  assert(loadedFixture.category == "Spot");

  // Dictionary should be updated with scene (MVR) priority category.
  auto entry = GdtfDictionary::Get("FixtureType");
  assert(entry.has_value());
  assert(entry->category == "Spot");

  cfg.Reset();
  MvrScene &scene2 = cfg.GetScene();
  scene2.layers[layer.uuid] = layer;
  Fixture fixtureWithoutCategory = fixture;
  fixtureWithoutCategory.category.clear();
  fixtureWithoutCategory.categorySource.clear();
  scene2.fixtures[fixtureWithoutCategory.uuid] = fixtureWithoutCategory;

  const std::filesystem::path noCategoryMvrPath = tempDir / "fixture_no_category.mvr";
  assert(exporter.ExportToFile(noCategoryMvrPath.string()));

  cfg.Reset();
  assert(importer.ImportFromFile(noCategoryMvrPath.string(), false, false));
  const auto &loadedNoCategory = cfg.GetScene().fixtures;
  assert(loadedNoCategory.at("fx1").category == "Spot");

  std::filesystem::remove_all(tempDir);
  std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") +
                          "/fixture.gdtf");
  return 0;
}
