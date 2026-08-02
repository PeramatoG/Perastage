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
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "fixture.h"
#include "fixture_label_overrides.h"
#include "filesystem_path_utils.h"
#include "gdtf_test_fixture_builder.h"
#include "gdtfdictionary.h"
#include "layer.h"
#include "matrixutils.h"
#include "projectutils.h"
#include "project_fixture_identity.h"
#include "project_symbol_cache_snapshot.h"
#include "sceneobject.h"
#include "scene_node_operations.h"
#include "support/zip_test_utils.h"
#include "support.h"
#include "truss.h"
#include "uuidutils.h"
#include "wx_path_utils.h"

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

// Reads every file entry from an immutable in-memory ZIP payload.
static std::map<std::string, std::vector<std::uint8_t>> ReadZipEntries(
    const std::string &archiveBytes) {
  wxMemoryInputStream input(archiveBytes.data(), archiveBytes.size());
  wxZipInputStream zip(input);
  std::map<std::string, std::vector<std::uint8_t>> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string bytes = ReadCurrentZipEntry(zip);
    entries.emplace(tests::zip::ToUtf8(entry->GetName(wxPATH_UNIX)),
                    std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
  }
  return entries;
}

// Reads every file entry from a project archive through the native wx path boundary.
static std::map<std::string, std::vector<std::uint8_t>> ReadProjectEntries(
    const std::filesystem::path &projectPath) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(projectPath));
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::map<std::string, std::vector<std::uint8_t>> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string bytes = ReadCurrentZipEntry(zip);
    entries.emplace(tests::zip::ToUtf8(entry->GetName(wxPATH_UNIX)),
                    std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
  }
  return entries;
}

// Rewrites a project with a missing or replacement symbol-cache manifest.
static void WriteProjectManifestVariant(
    const std::filesystem::path &sourcePath,
    const std::filesystem::path &destinationPath,
    const std::optional<std::string> &manifestText) {
  auto entries = ReadProjectEntries(sourcePath);
  entries.erase(symbol_cache::kProjectArchiveEntryName);
  if (manifestText) {
    entries.emplace(symbol_cache::kProjectArchiveEntryName,
                    std::vector<std::uint8_t>(manifestText->begin(),
                                              manifestText->end()));
  }
  wxFileOutputStream output(
      WxPathUtils::WxStringFromFilesystemPath(destinationPath));
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  for (const auto &[name, bytes] : entries) {
    assert(zip.PutNextEntry(name));
    if (!bytes.empty())
      zip.Write(bytes.data(), bytes.size());
    assert(zip.CloseEntry());
  }
  assert(zip.Close());
}

// Reads the fixture type name that the production GDTF loader restores.
static std::string ReadFixtureTypeName(
    const std::filesystem::path &gdtfPath) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(gdtfPath));
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetName().CmpNoCase("description.xml") != 0)
      continue;
    const std::string xml = ReadCurrentZipEntry(zip);
    tinyxml2::XMLDocument document;
    assert(document.Parse(xml.c_str(), xml.size()) == tinyxml2::XML_SUCCESS);
    const tinyxml2::XMLElement *fixtureType =
        document.FirstChildElement("GDTF");
    fixtureType = fixtureType
                      ? fixtureType->FirstChildElement("FixtureType")
                      : document.FirstChildElement("FixtureType");
    assert(fixtureType != nullptr);
    const char *name = fixtureType->Attribute("Name");
    assert(name != nullptr);
    return name;
  }
  assert(false);
  return {};
}

struct ProjectIdentityPayload {
  std::string configJson;
  std::string sceneXml;
};

// Compares every component of two persisted scene matrices.
static bool MatrixEqual(const Matrix &lhs, const Matrix &rhs) {
  for (const auto &pair : {std::pair{&lhs.u, &rhs.u},
                           std::pair{&lhs.v, &rhs.v},
                           std::pair{&lhs.w, &rhs.w},
                           std::pair{&lhs.o, &rhs.o}}) {
    for (size_t index = 0; index < 3; ++index) {
      if (std::fabs((*pair.first)[index] - (*pair.second)[index]) > 0.02f)
        return false;
    }
  }
  return true;
}

// Reads the two project payloads that must agree on fixture identity.
static ProjectIdentityPayload ReadProjectIdentityPayload(
    const std::filesystem::path &projectPath) {
  wxFileInputStream projectInput(
      WxPathUtils::WxStringFromFilesystemPath(projectPath));
  assert(projectInput.IsOk());
  wxZipInputStream projectZip(projectInput);
  ProjectIdentityPayload payload;
  std::string sceneBytes;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(projectZip.GetNextEntry())), entry) {
    const std::string bytes = ReadCurrentZipEntry(projectZip);
    if (entry->GetName() == "config.json")
      payload.configJson = bytes;
    else if (entry->GetName() == "scene.mvr")
      sceneBytes = bytes;
  }
  assert(!payload.configJson.empty());
  assert(!sceneBytes.empty());

  wxMemoryInputStream sceneInput(sceneBytes.data(), sceneBytes.size());
  wxZipInputStream sceneZip(sceneInput);
  while ((entry.reset(sceneZip.GetNextEntry())), entry) {
    const std::string bytes = ReadCurrentZipEntry(sceneZip);
    if (entry->GetName() == "GeneralSceneDescription.xml")
      payload.sceneXml = bytes;
  }
  assert(!payload.sceneXml.empty());
  return payload;
}

// Rewrites only config.json to model a legacy project package.
static void WriteProjectWithConfig(
    const std::filesystem::path &sourcePath,
    const std::filesystem::path &destinationPath,
    const std::string &configJson) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(sourcePath));
  assert(input.IsOk());
  wxZipInputStream sourceZip(input);
  wxFileOutputStream output(
      WxPathUtils::WxStringFromFilesystemPath(destinationPath));
  assert(output.IsOk());
  wxZipOutputStream destinationZip(output);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(sourceZip.GetNextEntry())), entry) {
    const std::string bytes = ReadCurrentZipEntry(sourceZip);
    auto *copy = new wxZipEntry(*entry);
    assert(destinationZip.PutNextEntry(copy));
    const std::string &written =
        entry->GetName() == "config.json" ? configJson : bytes;
    destinationZip.Write(written.data(), written.size());
    assert(destinationZip.CloseEntry());
  }
  assert(destinationZip.Close());
}

// Extracts the deterministic project fixture metadata signature.
static std::vector<std::pair<std::string, std::string>>
ReadProjectFixtureMetadata(const std::filesystem::path &projectPath) {
  wxFileInputStream projectInput(
      WxPathUtils::WxStringFromFilesystemPath(projectPath));
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
    const char *uuidAttribute = entry->Attribute("uuid");
    const char *colorAttribute = entry->Attribute("visualColorHex");
    assert(uuidAttribute != nullptr);
    const std::string uuid = uuidAttribute;
    const std::string color = colorAttribute ? colorAttribute : "";
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
        .WithPerastageGeneratedSymbols()
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
  GroupObject groupedTransformRoot;
  groupedTransformRoot.uuid = "60000000-0000-4000-8000-000000000001";
  groupedTransformRoot.name = "Grouped transform persistence";
  groupedTransformRoot.layer = layer.name;
  groupedTransformRoot.transform =
      MatrixUtils::EulerToMatrix(23.0f, 11.0f, 37.0f);
  groupedTransformRoot.transform.o = {1200.0f, -700.0f, 3400.0f};
  groupedTransformRoot.localTransform = groupedTransformRoot.transform;
  groupedTransformRoot.children = {{MvrNodeType::Truss, t.uuid},
                                   {MvrNodeType::SceneObject, o.uuid}};
  scene.groupObjects[groupedTransformRoot.uuid] = groupedTransformRoot;
  t.parentGroupUuid = groupedTransformRoot.uuid;
  t.localTransform = MatrixUtils::EulerToMatrix(7.0f, 19.0f, 5.0f);
  t.localTransform.o = {400.0f, 300.0f, -200.0f};
  t.hasLocalTransform = true;
  t.transform = MatrixUtils::Multiply(groupedTransformRoot.transform,
                                      t.localTransform);
  scene.trusses[t.uuid] = t;
  o.parentGroupUuid = groupedTransformRoot.uuid;
  o.localTransform = MatrixUtils::EulerToMatrix(3.0f, 13.0f, 17.0f);
  o.localTransform.o = {-500.0f, 250.0f, 100.0f};
  o.hasLocalTransform = true;
  o.transform = MatrixUtils::Multiply(groupedTransformRoot.transform,
                                      o.localTransform);
  scene.sceneObjects[o.uuid] = o;
  const Matrix originalGroupedTrussWorld = t.transform;
  const Matrix unchangedGroupedSiblingWorld = o.transform;
  Matrix editedGroupedTrussWorld =
      MatrixUtils::EulerToMatrix(41.0f, 29.0f, 31.0f);
  editedGroupedTrussWorld.o = {5100.0f, 2200.0f, -900.0f};
  assert(scene_node_operations::ApplyExactWorldTransform(
      scene, MvrNodeType::Truss, t.uuid, editedGroupedTrussWorld));
  assert(!MatrixEqual(originalGroupedTrussWorld,
                      scene.trusses.at(t.uuid).transform));
  assert(MatrixEqual(scene.groupObjects[groupedTransformRoot.uuid].transform,
                     groupedTransformRoot.transform));
  assert(MatrixEqual(scene.sceneObjects[o.uuid].transform,
                     unchangedGroupedSiblingWorld));
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
  sManual.loadSource = "Manual";
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

  std::filesystem::path temp = std::filesystem::temp_directory_path() /
                               std::filesystem::path(u8"símbolos_roundtrip.pstg");
    assert(cfg.SaveProject(PathUtils::PathToUtf8(temp)));

    const auto projectEntries = ReadProjectEntries(temp);
    assert(projectEntries.contains("config.json"));
    assert(projectEntries.contains("scene.mvr"));
    assert(projectEntries.contains(symbol_cache::kProjectArchiveEntryName));
    const std::string sceneArchiveBytes(projectEntries.at("scene.mvr").begin(),
                                        projectEntries.at("scene.mvr").end());
    const auto sceneEntries = ReadZipEntries(sceneArchiveBytes);
    assert(sceneEntries.contains("GeneralSceneDescription.xml"));
    const std::string sceneDescription(
        sceneEntries.at("GeneralSceneDescription.xml").begin(),
        sceneEntries.at("GeneralSceneDescription.xml").end());
    tinyxml2::XMLDocument savedSceneDocument;
    assert(savedSceneDocument.Parse(sceneDescription.c_str(),
                                    sceneDescription.size()) ==
           tinyxml2::XML_SUCCESS);
    tinyxml2::XMLElement *savedFixture =
        savedSceneDocument.RootElement()
            ->FirstChildElement("Scene")
            ->FirstChildElement("Layers")
            ->FirstChildElement("Layer")
            ->FirstChildElement("ChildList")
            ->FirstChildElement("Fixture");
    assert(savedFixture != nullptr);
    const char *savedGdtfText =
        savedFixture->FirstChildElement("GDTFSpec")->GetText();
    assert(savedGdtfText != nullptr);
    const std::string savedGdtfSpec = savedGdtfText;
    assert(sceneEntries.contains(savedGdtfSpec));
    const std::string packagedGdtfBytes(sceneEntries.at(savedGdtfSpec).begin(),
                                        sceneEntries.at(savedGdtfSpec).end());
    const auto packagedGdtfEntries = ReadZipEntries(packagedGdtfBytes);
    assert(packagedGdtfEntries.contains("models/svg/main.svg"));
    assert(packagedGdtfEntries.contains("models/svg/main_bottom.svg"));
    assert(packagedGdtfEntries.contains("models/svg_front/main.svg"));
    assert(packagedGdtfEntries.contains("models/svg_side/main.svg"));
    std::vector<symbol_cache::GdtfSemanticFingerprintEntry> fingerprintEntries;
    for (const auto &[path, bytes] : packagedGdtfEntries)
      fingerprintEntries.push_back({path, bytes});
    std::string fingerprintError;
    const std::string packagedFingerprint =
        symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(
            fingerprintEntries, fingerprintError);
    assert(!packagedFingerprint.empty());
    symbol_cache::SymbolCacheManifest savedManifest;
    std::string manifestError;
    assert(savedManifest.LoadFromJsonText(
        std::string(projectEntries.at(symbol_cache::kProjectArchiveEntryName).begin(),
                    projectEntries.at(symbol_cache::kProjectArchiveEntryName).end()),
        manifestError));
    assert(savedManifest.Entries().size() == 1);
    assert(savedManifest.Entries().front().gdtfSpec == savedGdtfSpec);
    assert(savedManifest.Entries().front().gdtfContentHash == packagedFingerprint);

    const std::string canonicalFixtureUuid =
        CanonicalizeUuid(nonCanonicalFixtureUuid);
    assert(!canonicalFixtureUuid.empty());
    const ProjectIdentityPayload firstPayload =
        ReadProjectIdentityPayload(temp);
    const nlohmann::json firstConfig =
        nlohmann::json::parse(firstPayload.configJson);
    const nlohmann::json firstOverrides = nlohmann::json::parse(
        firstConfig.at(project_identity::kFixtureLabelOverridesConfigKey)
            .get<std::string>());
    assert(firstOverrides.size() == 1);
    assert(firstOverrides.contains(canonicalFixtureUuid));
    assert(!firstOverrides.contains(nonCanonicalFixtureUuid));
    assert(firstPayload.sceneXml.find("uuid=\"" + canonicalFixtureUuid + "\"") !=
           std::string::npos);
    assert(firstPayload.sceneXml.find(nonCanonicalFixtureUuid) ==
           std::string::npos);
    assert(firstPayload.sceneXml.find(
               project_identity::kFixtureLabelOverridesConfigKey) ==
           std::string::npos);

    const auto collisionNormalization =
        project_identity::NormalizeFixtureLabelOverrides(
            std::string("{\"") + canonicalFixtureUuid +
                "\":{\"showLabelName\":[true,null,null]},\"" +
                nonCanonicalFixtureUuid +
                "\":{\"showLabelName\":[false,null,null],"
                "\"showLabelId\":[null,true,null]}}",
            {canonicalFixtureUuid});
    assert(collisionNormalization.migratedCount == 1);
    assert(collisionNormalization.collisionCount == 1);
    const nlohmann::json collisionOverrides =
        nlohmann::json::parse(*collisionNormalization.serializedOverrides);
    assert(collisionOverrides.size() == 1);
    assert(collisionOverrides.at(canonicalFixtureUuid)
               .at("showLabelName")
               .at(0) == true);
    assert(collisionOverrides.at(canonicalFixtureUuid)
               .at("showLabelId")
               .at(1) == true);

    MvrScene identityAuditScene;
    Fixture canonicalizableFixture;
    canonicalizableFixture.uuid = nonCanonicalFixtureUuid;
    identityAuditScene.fixtures.emplace("canonicalizable",
                                        canonicalizableFixture);
    Fixture invalidFixture;
    invalidFixture.uuid = "invalid-fixture-id";
    identityAuditScene.fixtures.emplace("invalid", invalidFixture);
    Fixture emptyFixture;
    identityAuditScene.fixtures.emplace("empty", emptyFixture);
    const auto recoverableBeforeCollision =
        project_identity::CollectRecoverableFixtureUuids(identityAuditScene);
    assert(recoverableBeforeCollision.size() == 1);
    assert(recoverableBeforeCollision.contains(canonicalFixtureUuid));
    Fixture duplicateFixture = canonicalizableFixture;
    duplicateFixture.uuid = canonicalFixtureUuid;
    identityAuditScene.fixtures.emplace("duplicate", duplicateFixture);
    assert(project_identity::CollectRecoverableFixtureUuids(identityAuditScene)
               .empty());

    cfg.Reset();

    assert(cfg.LoadProject(PathUtils::PathToUtf8(temp)));

    const auto &scene2 = cfg.GetScene();
    const auto &loadedGroupedTruss = scene2.trusses.at(t.uuid);
    assert(loadedGroupedTruss.parentGroupUuid == groupedTransformRoot.uuid);
    assert(MatrixEqual(loadedGroupedTruss.transform, editedGroupedTrussWorld));
    assert(!MatrixEqual(loadedGroupedTruss.transform,
                        originalGroupedTrussWorld));
    assert(MatrixEqual(scene2.sceneObjects.at(o.uuid).transform,
                       unchangedGroupedSiblingWorld));
    assert(scene2.groupObjects.at(groupedTransformRoot.uuid).children.size() ==
           2);
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
    assert(loadedManual.loadSource == "Manual");

    const auto &loadedInherited = scene2.supports.at("50000000-0000-4000-8000-000000000002");
    assert(loadedInherited.motorName == "CM Lodestar");
    assert(loadedInherited.motorFixtureUuid == "20000000-0000-4000-8000-000000000001");
    assert(!loadedInherited.useMotorDefaults);
    assert(loadedInherited.dummyPreset == "Lodestar 500kg");
    assert(loadedInherited.hoistDataSource == "Inherited");

    const auto &loaded = scene2.fixtures.at("20000000-0000-4000-8000-000000000001");
    assert(std::filesystem::path(loaded.gdtfSpec).filename() ==
           "Perastage@Original@Perastage.gdtf");
    assert(loaded.gdtfSpec == savedGdtfSpec);
    std::string reloadFingerprintError;
    const std::filesystem::path reloadedGdtfPath =
        PathUtils::PathFromUtf8(scene2.basePath) /
        PathUtils::PathFromUtf8(loaded.gdtfSpec);
    symbol_cache::ValidationRequest reloadRequest;
    Fixture productionReloadIdentity = loaded;
    productionReloadIdentity.typeName = ReadFixtureTypeName(reloadedGdtfPath);
    reloadRequest.fixtureKey = symbol_cache::BuildFixtureSymbolCacheKey(
        productionReloadIdentity);
    reloadRequest.fixtureTypeName = productionReloadIdentity.typeName;
    reloadRequest.gdtfSpec = loaded.gdtfSpec;
    reloadRequest.gdtfContentHash = symbol_cache::ComputeFileContentHash(
        PathUtils::PathToUtf8(reloadedGdtfPath), reloadFingerprintError);
    reloadRequest.requiredViews = symbol_cache::RequiredPerastageSymbolViews();
    assert(!reloadRequest.gdtfContentHash.empty());
    const auto reloadValidation =
        cfg.GetSymbolCacheManifest().ValidateFixture(reloadRequest);
    if (!reloadValidation.valid) {
      std::cerr << "Reload cache validation failed: "
                << symbol_cache::ValidationStatusName(reloadValidation.status)
                << " key=" << reloadRequest.fixtureKey
                << " spec=" << reloadRequest.gdtfSpec
                << " hash=" << reloadRequest.gdtfContentHash << '\n';
    }
    assert(reloadValidation.valid);
    assert(symbol_cache::PlanFixtureSymbolCacheMisses(
               cfg.GetSymbolCacheManifest(), {reloadRequest})
               .empty());

    const auto manifestBeforeFailedSave =
        cfg.GetSymbolCacheManifest().Entries();
    const auto dirtyStateBeforeFailedSave = cfg.CaptureDirtyState();
    std::error_code failedSaveCleanupError;
    std::filesystem::remove_all(tempDir / "missing-parent",
                                failedSaveCleanupError);
    const std::filesystem::path invalidSavePath =
        tempDir / "missing-parent" / "failed.pstg";
    assert(!cfg.SaveProject(PathUtils::PathToUtf8(invalidSavePath)));
    assert(cfg.GetSymbolCacheManifest().Entries().size() ==
           manifestBeforeFailedSave.size());
    assert(cfg.GetSymbolCacheManifest().Entries().front().gdtfContentHash ==
           manifestBeforeFailedSave.front().gdtfContentHash);
    const auto dirtyStateAfterFailedSave = cfg.CaptureDirtyState();
    assert(dirtyStateAfterFailedSave.revision ==
           dirtyStateBeforeFailedSave.revision);
    assert(dirtyStateAfterFailedSave.savedRevision ==
           dirtyStateBeforeFailedSave.savedRevision);
    assert(std::filesystem::path(loaded.originalMvrGdtfSpec).filename() ==
           "Perastage@Original@Perastage.gdtf");

    nlohmann::json legacyConfig = firstConfig;
    nlohmann::json legacyOverrides = firstOverrides;
    legacyOverrides[nonCanonicalFixtureUuid] =
        legacyOverrides.at(canonicalFixtureUuid);
    legacyOverrides.erase(canonicalFixtureUuid);
    legacyConfig[project_identity::kFixtureLabelOverridesConfigKey] =
        legacyOverrides.dump();
    const std::filesystem::path legacyProject =
        std::filesystem::temp_directory_path() / "roundtrip_test_legacy.pera";
    WriteProjectWithConfig(temp, legacyProject, legacyConfig.dump(2));
    cfg.Reset();
    assert(cfg.LoadProject(legacyProject.string()));
    const auto recoveredOverrides =
        viewer2d::LoadFixtureLabelOverrides(cfg);
    assert(recoveredOverrides.size() == 1);
    assert(recoveredOverrides.contains(canonicalFixtureUuid));
    assert(!recoveredOverrides.contains(nonCanonicalFixtureUuid));
    assert(*recoveredOverrides.at(canonicalFixtureUuid).showLabelName[0]);

    const auto firstMetadata = ReadProjectFixtureMetadata(temp);
    GdtfDictionary::Save({});
    const std::filesystem::path secondProject =
        std::filesystem::temp_directory_path() / "roundtrip_test_second.pera";
    assert(cfg.SaveProject(secondProject.string()));
    const ProjectIdentityPayload secondPayload =
        ReadProjectIdentityPayload(secondProject);
    const nlohmann::json secondConfig =
        nlohmann::json::parse(secondPayload.configJson);
    const nlohmann::json secondOverrides = nlohmann::json::parse(
        secondConfig.at(project_identity::kFixtureLabelOverridesConfigKey)
            .get<std::string>());
    assert(secondOverrides == firstOverrides);
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

    const std::filesystem::path missingManifestProject =
        tempDir / std::filesystem::path(u8"sin_manifiesto.pstg");
    const std::filesystem::path malformedManifestProject =
        tempDir / std::filesystem::path(u8"manifiesto_inválido.pstg");
    WriteProjectManifestVariant(temp, missingManifestProject, std::nullopt);
    WriteProjectManifestVariant(temp, malformedManifestProject,
                                std::string("{invalid"));
    cfg.Reset();
    assert(cfg.LoadProject(PathUtils::PathToUtf8(missingManifestProject)));
    assert(!cfg.GetSymbolCacheManifest().HasLoadedManifest());
    cfg.Reset();
    assert(cfg.LoadProject(PathUtils::PathToUtf8(malformedManifestProject)));
    assert(!cfg.GetSymbolCacheManifest().HasLoadedManifest());

    std::filesystem::remove(temp);
    std::filesystem::remove(legacyProject);
    std::filesystem::remove(secondProject);
    std::filesystem::remove(missingManifestProject);
    std::filesystem::remove(malformedManifestProject);
  std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") +
                          "/dict.gdtf");
    GdtfDictionary::Save({});
    std::filesystem::remove_all(tempDir);
    return 0;
}
