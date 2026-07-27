/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "fixture.h"
#include "fixture_label_overrides.h"
#include "gdtf_test_fixture_builder.h"
#include "gdtfdictionary.h"
#include "layer.h"
#include "projectutils.h"
#include "sceneobject.h"
#include "support.h"
#include "truss.h"
#include "uuidutils.h"

// Reads all bytes from the current ZIP entry.
static std::string ReadCurrentZipEntry(wxZipInputStream &zip) {
  std::string bytes;
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t count = zip.LastRead();
    if (count == 0)
      break;
    bytes.append(buffer, count);
  }
  return bytes;
}

// Extracts the deterministic project fixture metadata signature.
static std::vector<std::pair<std::string, std::string>>
ReadProjectFixtureMetadata(const std::filesystem::path &projectPath) {
  wxFileInputStream projectInput(projectPath.string());
  assert(projectInput.IsOk());
  wxZipInputStream projectZip(projectInput);
  std::string sceneBytes;
  std::unique_ptr<wxZipEntry> projectEntry;
  while ((projectEntry.reset(projectZip.GetNextEntry())), projectEntry) {
    if (projectEntry->GetName() == "scene.mvr")
      sceneBytes = ReadCurrentZipEntry(projectZip);
    else
      ReadCurrentZipEntry(projectZip);
  }
  assert(!sceneBytes.empty());

  wxMemoryInputStream sceneInput(sceneBytes.data(), sceneBytes.size());
  wxZipInputStream sceneZip(sceneInput);
  std::string sceneXml;
  std::unique_ptr<wxZipEntry> sceneEntry;
  while ((sceneEntry.reset(sceneZip.GetNextEntry())), sceneEntry) {
    if (sceneEntry->GetName() == "GeneralSceneDescription.xml")
      sceneXml = ReadCurrentZipEntry(sceneZip);
    else
      ReadCurrentZipEntry(sceneZip);
  }
  assert(!sceneXml.empty());

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(sceneXml.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root =
      doc.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);
  tinyxml2::XMLElement *userData = root->FirstChildElement("UserData");
  assert(userData != nullptr);
  tinyxml2::XMLElement *data = userData->FirstChildElement("Data");
  assert(data != nullptr);
  tinyxml2::XMLElement *map =
      data->FirstChildElement("ProjectFixtureMetadataMap");
  assert(map != nullptr);
  assert(map->NextSiblingElement("ProjectFixtureMetadataMap") == nullptr);
  assert(std::string(map->Attribute("schemaVersion")) == "1.0");

  std::vector<std::pair<std::string, std::string>> signature;
  std::set<std::string> uniqueUuids;
  for (tinyxml2::XMLElement *entry =
           map->FirstChildElement("ProjectFixtureMetadata");
       entry; entry = entry->NextSiblingElement("ProjectFixtureMetadata")) {
    const std::string uuid = entry->Attribute("uuid");
    const std::string color = entry->Attribute("visualColorHex");
    assert(CanonicalizeUuid(uuid) == uuid);
    assert(uniqueUuids.insert(uuid).second);
    signature.emplace_back(uuid, color);
  }
  assert(std::is_sorted(signature.begin(), signature.end()));
  return signature;
}

// Verifies project persistence and dictionary normalization round trips.
int main() {
    wxInitializer initializer;
    assert(initializer.IsOk());

    auto &cfg = ConfigManager::Get();
    cfg.Reset();
    MvrScene &scene = cfg.GetScene();

    Layer layer;
    layer.uuid = "10000000-0000-4000-8000-000000000001";
    layer.name = "Layer1";
    layer.color = "#112233";
    scene.layers[layer.uuid] = layer;

    // Prepare dummy GDTF files
  std::filesystem::path tempDir =
      std::filesystem::temp_directory_path() / "gdtf_roundtrip";
    std::filesystem::create_directories(tempDir);
    tests::gdtf::BuildMinimalValidFixture()
        .WithFixtureIdentity("Original", "Perastage",
                             "11111111-1111-4111-8111-111111111111")
        .WriteArchive(tempDir / "orig.gdtf");
    tests::gdtf::BuildMinimalValidFixture()
        .WithFixtureIdentity("Dictionary", "Perastage",
                             "22222222-2222-4222-8222-222222222222")
        .WriteArchive(tempDir / "dict.gdtf");
    scene.basePath = tempDir.string();

    // Dictionary entry that should NOT be applied on load
    GdtfDictionary::Update("FixtureType", (tempDir / "dict.gdtf").string(), "");
    auto fixtureTypeCanonical = GdtfDictionary::Get("FixtureType");
    assert(fixtureTypeCanonical.has_value());
    const auto fixtureTypeLower = GdtfDictionary::Get("fixturetype");
    assert(fixtureTypeLower.has_value());
    assert(fixtureTypeLower->path == fixtureTypeCanonical->path);
    const auto fixtureTypeSpaced = GdtfDictionary::Get("Fixture   Type");
    assert(fixtureTypeSpaced.has_value());
    assert(fixtureTypeSpaced->path == fixtureTypeCanonical->path);
    std::ofstream(tempDir / "Dummy 1ch.gdtf") << "dummy";
    tests::gdtf::BuildMinimalValidFixture()
        .WithFixtureIdentity("Shared A", "Perastage",
                             "aaaaaaaa-1111-4111-8111-111111111111")
        .WithDmxMode("Mode A", "Root")
        .WriteArchive(tempDir / "shared.gdtf");
    tests::gdtf::BuildMinimalValidFixture()
        .WithFixtureIdentity("Shared B", "Perastage",
                             "bbbbbbbb-2222-4222-8222-222222222222")
        .WithDmxMode("Mode B", "Root")
        .WriteArchive(tempDir / "subdir" / "shared.gdtf");
  GdtfDictionary::Update("Dummy 1ch", (tempDir / "Dummy 1ch.gdtf").string(),
                         "");
    auto dummyEntry = GdtfDictionary::Get("Dummy 1ch");
    assert(!dummyEntry.has_value());
    GdtfDictionary::Update("Dummy 1ch", (tempDir / "dict.gdtf").string(), "");
    dummyEntry = GdtfDictionary::Get("Dummy 1ch");
    assert(!dummyEntry.has_value());
  GdtfDictionary::Update("Some Type", (tempDir / "Dummy 1ch.gdtf").string(),
                         "");
    auto dummyPathEntry = GdtfDictionary::Get("Some Type");
    assert(!dummyPathEntry.has_value());
    GdtfDictionary::UpdateCategory("TextOnlyType", "Spot");
    auto textOnlyEntry = GdtfDictionary::Get("TextOnlyType");
    assert(textOnlyEntry.has_value());
    assert(textOnlyEntry->category == "Spot");
    assert(textOnlyEntry->path.empty());
  std::unordered_map<std::string, GdtfDictionary::Entry>
      categoryPropagationDict;
    categoryPropagationDict["Type A"] = {
        (tempDir / "shared.gdtf").string(), "Mode A", "OldA", "", "", ""};
    categoryPropagationDict["Type B"] = {
      (tempDir / "subdir" / "shared.gdtf").string(),
      "Mode B",
      "OldB",
      "",
      "",
      ""};
    assert(GdtfDictionary::Save(categoryPropagationDict));
    GdtfDictionary::UpdateCategoriesBulk({{"Type A", "Wash"}});
    const auto categoryPropagationA = GdtfDictionary::Get("Type A");
    const auto categoryPropagationB = GdtfDictionary::Get("Type B");
    assert(categoryPropagationA.has_value());
    assert(categoryPropagationB.has_value());
    assert(categoryPropagationA->category == "Wash");
    assert(categoryPropagationB->category == "Wash");

  Fixture f;
  f.uuid = "20000000-0000-4000-8000-000000000001";
  f.instanceName = "Fixture";
  f.layer = layer.name;
  f.typeName = "FixtureType";
  f.gdtfSpec = "orig.gdtf";
  f.visualColorHex = "#445566";
  f.fixtureIdText = "S101A";
  f.fixtureIdNumeric = 101;
  f.fixtureId = 101;
  scene.fixtures[f.uuid] = f;
  Fixture f2;
  f2.uuid = "20000000-0000-4000-8000-000000000002";
  f2.instanceName = "Fixture 2";
  f2.layer = layer.name;
  f2.typeName = "FixtureType";
  f2.gdtfSpec = "orig.gdtf";
  f2.fixtureIdText = "S101B";
  f2.fixtureIdNumeric = 101;
  f2.fixtureId = 101;
  scene.fixtures[f2.uuid] = f2;
  const std::string nonCanonicalFixtureUuid =
      "A0B1C2D3-E4F5-4678-9ABC-DEF012345678";
  Fixture f3;
  f3.uuid = nonCanonicalFixtureUuid;
  f3.instanceName = "Fixture 3";
  f3.layer = layer.name;
  f3.typeName = "FixtureType";
  f3.gdtfSpec = "orig.gdtf";
  f3.fixtureIdText = "S101C";
  f3.fixtureIdNumeric = 101;
  f3.fixtureId = 101;
  scene.fixtures[f3.uuid] = f3;
  Fixture f4;
  f4.uuid = "20000000-0000-4000-8000-000000000004";
  f4.instanceName = "Edited ID Fixture";
  f4.layer = layer.name;
  f4.typeName = "FixtureType";
  f4.gdtfSpec = "orig.gdtf";
  f4.visualColorHex = "#778899";
  f4.fixtureIdText = "Imported ID";
  f4.fixtureIdNumeric = 44;
  f4.fixtureId = 707;
  scene.fixtures[f4.uuid] = f4;
    viewer2d::ApplyShowLabelNameOverride(cfg, {nonCanonicalFixtureUuid}, 0, true);
  Truss t;
  t.uuid = "30000000-0000-4000-8000-000000000001";
  t.name = "Truss";
  t.layer = layer.name;
  scene.trusses[t.uuid] = t;
  SceneObject o;
  o.uuid = "40000000-0000-4000-8000-000000000001";
  o.name = "Object";
  o.layer = layer.name;
  scene.sceneObjects[o.uuid] = o;
    SceneObject cylinderObj;
    cylinderObj.uuid = "40000000-0000-4000-8000-000000000002";
    cylinderObj.name = "Cylinder";
    cylinderObj.layer = layer.name;
    GeometryInstance cylinderGeometry;
  cylinderGeometry.modelFile =
      "primitive:cylinder;top=200.000000;bottom=450.000000;height=1200.000000";
    cylinderObj.geometries.push_back(cylinderGeometry);
    cylinderObj.modelFile = cylinderGeometry.modelFile;
    scene.sceneObjects[cylinderObj.uuid] = cylinderObj;

    SceneObject legacyPipeObj;
    legacyPipeObj.uuid = "40000000-0000-4000-8000-000000000003";
    legacyPipeObj.name = "Legacy Pipe";
    legacyPipeObj.layer = layer.name;
    GeometryInstance legacyPipeGeometry;
    legacyPipeGeometry.modelFile = "primitive:cylinder";
    legacyPipeObj.geometries.push_back(legacyPipeGeometry);
    legacyPipeObj.modelFile = legacyPipeGeometry.modelFile;
    legacyPipeObj.transform.u = {0.0f, 0.0f, -0.05f};
    legacyPipeObj.transform.v = {0.0f, 0.05f, 0.0f};
    legacyPipeObj.transform.w = {14.0f, 0.0f, 0.0f};
    legacyPipeObj.transform.o = {0.0f, 0.0f, 0.0f};
    scene.sceneObjects[legacyPipeObj.uuid] = legacyPipeObj;

  Support sManual;
  sManual.uuid = "50000000-0000-4000-8000-000000000001";
  sManual.name = "Manual Hoist";
  sManual.layer = layer.name;
  sManual.motorName = "ChainMaster D8+";
  sManual.motorManufacturer = "ChainMaster";
  sManual.motorModel = "D8+";
  sManual.dummyPreset = "D8+ 1000kg";
  sManual.hoistDataSource = "Manual";
  sManual.hoistFunction = "Audio";
  sManual.capacityKg = 700.0f;
  sManual.weightKg = 40.0f;
  sManual.loadKg = 325.0f;
  scene.supports[sManual.uuid] = sManual;

  Support sInherited;
  sInherited.uuid = "50000000-0000-4000-8000-000000000002";
  sInherited.name = "Inherited Hoist";
  sInherited.layer = layer.name;
  sInherited.motorName = "CM Lodestar";
  sInherited.motorFixtureUuid = "20000000-0000-4000-8000-000000000001";
  sInherited.useMotorDefaults = false;
  sInherited.dummyPreset = "Lodestar 500kg";
  sInherited.hoistDataSource = "Inherited";
  scene.supports[sInherited.uuid] = sInherited;

  std::filesystem::path temp =
      std::filesystem::temp_directory_path() / "roundtrip_test.pera";
    assert(cfg.SaveProject(temp.string()));

    cfg.Reset();

    assert(cfg.LoadProject(temp.string()));

    const auto &scene2 = cfg.GetScene();
    assert(scene2.fixtures.size() == 4);
    assert(scene2.trusses.size() == 1);
    assert(scene2.sceneObjects.size() == 3);
    assert(scene2.supports.size() == 2);
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000001").instanceName == "Fixture");
    assert(scene2.trusses.at("30000000-0000-4000-8000-000000000001").name == "Truss");
    assert(scene2.sceneObjects.at("40000000-0000-4000-8000-000000000001").name == "Object");
    assert(scene2.sceneObjects.at("40000000-0000-4000-8000-000000000002").geometries.size() == 1);
    const std::string loadedCylinderToken =
        scene2.sceneObjects.at("40000000-0000-4000-8000-000000000002").geometries.front().modelFile;
    assert(loadedCylinderToken.find("primitive:cylinder") == 0);
    assert(loadedCylinderToken.find("top=200") != std::string::npos);
    assert(loadedCylinderToken.find("bottom=450") != std::string::npos);
    const std::string loadedLegacyPipeToken =
        scene2.sceneObjects.at("40000000-0000-4000-8000-000000000003").geometries.front().modelFile;
    assert(loadedLegacyPipeToken == "primitive:cylinder");
  assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000001").visualColorHex == "#445566");
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000002")
               .visualColorHex.empty());
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000001").fixtureIdText == "S101A");
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000001").fixtureIdNumeric == 101);
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000002").fixtureIdText == "S101B");
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000002")
               .fixtureIdNumeric > 0);
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000002")
               .fixtureIdNumeric != 101);
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000004").fixtureId == 707);
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000004").fixtureIdNumeric == 707);
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000004").fixtureIdText == "707");
    assert(scene2.fixtures.at("20000000-0000-4000-8000-000000000004")
               .visualColorHex == "#778899");
  const std::string canonicalFixtureUuid =
      CanonicalizeUuid(nonCanonicalFixtureUuid);
    assert(!canonicalFixtureUuid.empty());
    assert(scene2.fixtures.count(canonicalFixtureUuid) == 1);
    const auto fixtureOverrides = viewer2d::LoadFixtureLabelOverrides(cfg);
    assert(fixtureOverrides.count(canonicalFixtureUuid) == 1);
    const auto &fixture3Override = fixtureOverrides.at(canonicalFixtureUuid);
    assert(fixture3Override.showLabelName[0].has_value());
    assert(*fixture3Override.showLabelName[0]);
    assert(scene2.layers.at("10000000-0000-4000-8000-000000000001").color == "#112233");

    const auto &loadedManual = scene2.supports.at("50000000-0000-4000-8000-000000000001");
    assert(loadedManual.motorName == "ChainMaster D8+");
    assert(loadedManual.motorManufacturer == "ChainMaster");
    assert(loadedManual.motorModel == "D8+");
    assert(loadedManual.dummyPreset == "D8+ 1000kg");
    assert(loadedManual.hoistDataSource == "Manual");
    assert(loadedManual.capacityKg == 700.0f);
    assert(loadedManual.weightKg == 40.0f);
    assert(loadedManual.loadKg == 325.0f);

    const auto &loadedInherited = scene2.supports.at("50000000-0000-4000-8000-000000000002");
    assert(loadedInherited.motorName == "CM Lodestar");
    assert(loadedInherited.motorFixtureUuid == "20000000-0000-4000-8000-000000000001");
    assert(!loadedInherited.useMotorDefaults);
    assert(loadedInherited.dummyPreset == "Lodestar 500kg");
    assert(loadedInherited.hoistDataSource == "Inherited");

    const auto &loaded = scene2.fixtures.at("20000000-0000-4000-8000-000000000001");
    assert(std::filesystem::path(loaded.gdtfSpec).filename() == "orig.gdtf");
  assert(std::filesystem::path(loaded.originalMvrGdtfSpec).filename() ==
         "orig.gdtf");

    const auto firstMetadata = ReadProjectFixtureMetadata(temp);
    GdtfDictionary::Save({});
    const std::filesystem::path secondProject =
        std::filesystem::temp_directory_path() / "roundtrip_test_second.pera";
    assert(cfg.SaveProject(secondProject.string()));
    const auto secondMetadata = ReadProjectFixtureMetadata(secondProject);
    assert(secondMetadata == firstMetadata);

    cfg.Reset();
    assert(cfg.LoadProject(secondProject.string()));
    const auto &scene3 = cfg.GetScene();
    assert(scene3.fixtures.at("20000000-0000-4000-8000-000000000001")
               .visualColorHex == "#445566");
    assert(scene3.fixtures.at("20000000-0000-4000-8000-000000000002")
               .visualColorHex.empty());
    assert(scene3.fixtures.at("20000000-0000-4000-8000-000000000004")
               .visualColorHex == "#778899");
    assert(scene3.fixtures.at("20000000-0000-4000-8000-000000000001")
               .fixtureIdText == "S101A");
    assert(scene3.fixtures.at("20000000-0000-4000-8000-000000000002")
               .fixtureIdText == "S101B");
    std::set<int> fixtureNumericIds;
    for (const auto &[uuid, fixture] : scene3.fixtures) {
      (void)uuid;
      assert(fixture.fixtureIdNumeric > 0);
      assert(fixtureNumericIds.insert(fixture.fixtureIdNumeric).second);
    }

    std::filesystem::remove(temp);
    std::filesystem::remove(secondProject);
  std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") +
                          "/dict.gdtf");
    GdtfDictionary::Save({});
    std::filesystem::remove_all(tempDir);
    return 0;
}
