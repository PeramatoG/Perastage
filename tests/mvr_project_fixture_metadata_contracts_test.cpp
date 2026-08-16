/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "fixture.h"
#include "gdtf_test_fixture_builder.h"
#include "layer.h"
#include "mvr_export_options.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "uuidutils.h"
#include "wx_path_utils.h"

namespace fs = std::filesystem;

struct ArchiveSnapshot {
  std::unordered_map<std::string, std::string> entries;
  std::string sceneXml;
};

struct SceneSignature {
  std::vector<std::pair<std::string, std::string>> metadata;
  std::map<std::string, std::string> gdtfByFixtureName;
  std::map<std::string, std::pair<std::string, int>> idsByFixtureName;
};

// Removes the test workspace when the executable exits.
class ScopedWorkspace {
public:
  explicit ScopedWorkspace(fs::path path) : path_(std::move(path)) {
    std::error_code ec;
    fs::remove_all(path_, ec);
    fs::create_directories(path_, ec);
    assert(!ec);
  }

  ~ScopedWorkspace() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path &Path() const { return path_; }

private:
  fs::path path_;
};

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

// Reads archive entries from a file path.
static std::unordered_map<std::string, std::string>
ReadArchiveEntries(const fs::path &path) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(path));
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    const std::string name = entry->GetName().ToStdString();
    assert(entries.emplace(name, ReadCurrentZipEntry(zip)).second);
  }
  return entries;
}

// Reads archive entries from an in-memory byte buffer.
static std::unordered_map<std::string, std::string>
ReadArchiveEntries(const std::string &bytes) {
  wxMemoryInputStream input(bytes.data(), bytes.size());
  wxZipInputStream zip(input);
  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    const std::string name = entry->GetName().ToStdString();
    assert(entries.emplace(name, ReadCurrentZipEntry(zip)).second);
  }
  return entries;
}

// Reads an MVR archive and its scene XML.
static ArchiveSnapshot ReadMvr(const fs::path &path) {
  ArchiveSnapshot snapshot;
  snapshot.entries = ReadArchiveEntries(path);
  const auto sceneIt = snapshot.entries.find("GeneralSceneDescription.xml");
  assert(sceneIt != snapshot.entries.end());
  snapshot.sceneXml = sceneIt->second;
  return snapshot;
}

// Reads the embedded MVR from a project archive.
static ArchiveSnapshot ReadProjectMvr(const fs::path &path) {
  const auto projectEntries = ReadArchiveEntries(path);
  const auto sceneIt = projectEntries.find("scene.mvr");
  assert(sceneIt != projectEntries.end());
  ArchiveSnapshot snapshot;
  snapshot.entries = ReadArchiveEntries(sceneIt->second);
  const auto xmlIt = snapshot.entries.find("GeneralSceneDescription.xml");
  assert(xmlIt != snapshot.entries.end());
  snapshot.sceneXml = xmlIt->second;
  return snapshot;
}

// Compares referenced payload contents while ignoring ZIP container metadata.
static void AssertCanonicalResourcePayloadEqual(
    const std::string &entryName, const std::string &firstBytes,
    const std::string &secondBytes) {
  if (fs::path(entryName).extension() == ".gdtf") {
    assert(ReadArchiveEntries(firstBytes) == ReadArchiveEntries(secondBytes));
    return;
  }
  assert(firstBytes == secondBytes);
}

// Returns the ASCII-lowercase representation of an archive identity.
static std::string FoldAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

// Parses fixture IDs, GDTF references, and project metadata from scene XML.
static SceneSignature ParseSceneSignature(const std::string &xml,
                                          bool expectProjectMap) {
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root =
      doc.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);

  SceneSignature signature;
  int projectMapCount = 0;
  for (tinyxml2::XMLElement *userData = root->FirstChildElement("UserData");
       userData; userData = userData->NextSiblingElement("UserData")) {
    for (tinyxml2::XMLElement *data = userData->FirstChildElement("Data"); data;
         data = data->NextSiblingElement("Data")) {
      const std::string provider =
          data->Attribute("provider") ? data->Attribute("provider") : "";
      const std::string version =
          data->Attribute("ver") ? data->Attribute("ver") : "";
      if (FoldAscii(provider) != "perastage" || version != "1.0")
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("ProjectFixtureMetadataMap");
           map; map = map->NextSiblingElement("ProjectFixtureMetadataMap")) {
        ++projectMapCount;
        assert(std::string(map->Attribute("schemaVersion")) == "1.0");
        std::set<std::string> uuids;
        for (tinyxml2::XMLElement *entry =
                 map->FirstChildElement("ProjectFixtureMetadata");
             entry;
             entry = entry->NextSiblingElement("ProjectFixtureMetadata")) {
          const std::string uuid = entry->Attribute("uuid");
          const std::string color = entry->Attribute("visualColorHex")
                                        ? entry->Attribute("visualColorHex")
                                        : "";
          assert(CanonicalizeUuid(uuid) == uuid);
          assert(uuids.insert(uuid).second);
          signature.metadata.emplace_back(uuid, color);
        }
      }
    }
  }
  assert(projectMapCount == (expectProjectMap ? 1 : 0));
  assert(std::is_sorted(signature.metadata.begin(), signature.metadata.end()));

  std::vector<tinyxml2::XMLElement *> stack{root};
  while (!stack.empty()) {
    tinyxml2::XMLElement *node = stack.back();
    stack.pop_back();
    if (std::string(node->Name()) == "Fixture") {
      const std::string name =
          node->Attribute("name") ? node->Attribute("name") : "";
      tinyxml2::XMLElement *spec = node->FirstChildElement("GDTFSpec");
      tinyxml2::XMLElement *id = node->FirstChildElement("FixtureID");
      tinyxml2::XMLElement *numeric =
          node->FirstChildElement("FixtureIDNumeric");
      assert(!name.empty() && spec && spec->GetText() && id && id->GetText() &&
             numeric && numeric->GetText());
      signature.gdtfByFixtureName[name] = spec->GetText();
      signature.idsByFixtureName[name] =
          {id->GetText(), std::stoi(numeric->GetText())};
    }
    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      stack.push_back(child);
  }
  return signature;
}

// Verifies one legal root UserData and returns matching foreign provider blocks.
static int CountForeignProviderBlocks(const std::string &xml,
                                      const std::string &provider) {
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root =
      doc.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);
  tinyxml2::XMLElement *userData = root->FirstChildElement("UserData");
  assert(userData != nullptr);
  assert(userData->NextSiblingElement("UserData") == nullptr);
  int count = 0;
  for (tinyxml2::XMLElement *data = userData->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    if (data->Attribute("provider") != nullptr &&
        provider == data->Attribute("provider")) {
      ++count;
      tinyxml2::XMLElement *payload = data->FirstChildElement("VendorPayload");
      assert(payload != nullptr);
      assert(payload->IntAttribute("meaning") == 42);
    }
  }
  return count;
}

// Reads the FixtureType name and UUID from a GDTF payload.
static std::pair<std::string, std::string>
ReadGdtfIdentity(const std::string &archiveBytes) {
  const auto entries = ReadArchiveEntries(archiveBytes);
  const auto descriptionIt = entries.find("description.xml");
  assert(descriptionIt != entries.end());
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(descriptionIt->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  assert(fixtureType != nullptr);
  fixtureType = fixtureType->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);
  return {fixtureType->Attribute("Name"),
          fixtureType->Attribute("FixtureTypeID")};
}

// Adds one fixture to the minimal contract scene.
static void AddFixture(MvrScene &scene, const std::string &uuid,
                       const std::string &name, const fs::path &gdtfPath,
                       int numericId, const std::string &textId,
                       const std::string &color) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.instanceName = name;
  fixture.layer = "Contracts";
  fixture.typeName = "Shared Type";
  fixture.gdtfSpec = gdtfPath.string();
  fixture.fixtureId = numericId;
  fixture.fixtureIdNumeric = numericId;
  fixture.fixtureIdText = textId;
  fixture.unitNumber = numericId + 1000;
  fixture.visualColorHex = color;
  fixture.visualColorState = color.empty()
                                 ? FixtureProjectColorState::ExplicitEmpty
                                 : FixtureProjectColorState::Present;
  fixture.address = std::to_string(scene.fixtures.size() + 1) + ".1";
  scene.fixtures.emplace(uuid, std::move(fixture));
}

// Verifies colors and fixture identifiers after a project restore.
static void AssertRestoredScene(const MvrScene &scene) {
  const Fixture &fixtureA =
      scene.fixtures.at("20000000-0000-4000-8000-000000000001");
  const Fixture &fixtureB =
      scene.fixtures.at("20000000-0000-4000-8000-000000000002");
  const Fixture &fixtureC =
      scene.fixtures.at("20000000-0000-4000-8000-000000000003");
  assert(fixtureA.visualColorHex == "#445566");
  assert(fixtureB.visualColorHex.empty());
  assert(fixtureC.visualColorHex == "#778899");
  assert(fixtureA.fixtureIdText == "S101A");
  assert(fixtureB.fixtureIdText == "S101B");
  assert(fixtureA.fixtureId == 101 && fixtureA.fixtureIdNumeric == 101);
  assert(fixtureB.fixtureId == 101 && fixtureB.fixtureIdNumeric == 101);
  assert(fixtureA.unitNumber == 1101);
  assert(fixtureB.unitNumber == 1101);
  assert(fixtureC.fixtureId == 101 && fixtureC.fixtureIdNumeric == 101);
  assert(fixtureC.fixtureIdText.empty());
  const Fixture &edited =
      scene.fixtures.at("20000000-0000-4000-8000-000000000004");
  assert(edited.fixtureId == 909);
  assert(edited.fixtureIdText == "Old imported ID");
  assert(edited.fixtureIdNumeric == 12);
  const Fixture &zero =
      scene.fixtures.at("20000000-0000-4000-8000-000000000006");
  assert(zero.fixtureId == 0);
  assert(zero.fixtureIdNumeric == 0);
  assert(zero.fixtureIdText == "0");
  assert(zero.unitNumber == 0);
}

// Runs the isolated Phase 4F exporter and project persistence contracts.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  ScopedWorkspace workspace(fs::temp_directory_path() /
                            "mvr_project_fixture_metadata_contracts_test");
  const fs::path libraryPath = workspace.Path() / "library";
  fs::create_directories(libraryPath / "fixtures");
  assert(wxSetEnv("PERASTAGE_LIBRARY_PATH", libraryPath.string()));

  const fs::path caseAPath = workspace.Path() / "case_a" / "CaseOnly.gdtf";
  const fs::path caseBPath = workspace.Path() / "case_b" / "caseonly.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Case A", "Acme",
                           "cccccccc-cccc-4ccc-8ccc-ccccccccccc3")
      .WriteArchive(caseAPath);
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Case B", "Acme",
                           "dddddddd-dddd-4ddd-8ddd-ddddddddddd4")
      .WriteArchive(caseBPath);

  ConfigManager &config = ConfigManager::Get();
  config.Reset();
  MvrScene &scene = config.GetScene();
  scene.basePath = workspace.Path().string();
  Layer layer;
  layer.uuid = "10000000-0000-4000-8000-000000000001";
  layer.name = "Contracts";
  scene.layers.emplace(layer.uuid, layer);
  scene.opaqueUserDataBlocks.push_back(
      {"SomeOtherApplication", "2.5",
       "<Data provider=\"SomeOtherApplication\" ver=\"2.5\">"
       "<VendorPayload meaning=\"42\"><Nested>preserved</Nested>"
       "</VendorPayload></Data>"});
  scene.opaqueUserDataBlocks.push_back(
      {"MalformedProvider", "1.0", "<Data provider=\"MalformedProvider\">"});
  AddFixture(scene, "20000000-0000-4000-8000-000000000001", "Fixture A",
             caseAPath, 101, "S101A", "#445566");
  AddFixture(scene, "20000000-0000-4000-8000-000000000002", "Fixture B",
             caseAPath, 101, "S101B", "");
  AddFixture(scene, "20000000-0000-4000-8000-000000000003", "Fixture C",
             caseAPath, 101, "", "#778899");
  AddFixture(scene, "20000000-0000-4000-8000-000000000004", "Edited ID",
             caseAPath, 12, "Old imported ID", "");
  scene.fixtures.at("20000000-0000-4000-8000-000000000004").fixtureId = 909;
  AddFixture(scene, "20000000-0000-4000-8000-000000000005", "Case B",
             caseBPath, 202, "Case B ID", "");
  AddFixture(scene, "20000000-0000-4000-8000-000000000006", "Zero ID",
             caseAPath, 0, "0", "");
  scene.fixtures.at("20000000-0000-4000-8000-000000000006").unitNumber = 0;

  const std::map<std::string, std::pair<int, std::string>> editableIds = {
      {"20000000-0000-4000-8000-000000000001", {101, "S101A"}},
      {"20000000-0000-4000-8000-000000000002", {101, "S101B"}},
      {"20000000-0000-4000-8000-000000000003", {101, ""}}};

  MvrExporter exporter;
  const fs::path standalonePath = workspace.Path() / "standalone.mvr";
  assert(exporter.ExportToFile(standalonePath.string(), MvrExportOptions{}));
  assert(std::any_of(exporter.GetExportWarnings().begin(),
                     exporter.GetExportWarnings().end(),
                     [](const std::string &warning) {
                       return warning.find("MalformedProvider") !=
                              std::string::npos;
                     }));
  const ArchiveSnapshot standalone = ReadMvr(standalonePath);
  const SceneSignature standaloneSignature =
      ParseSceneSignature(standalone.sceneXml, true);
  assert(CountForeignProviderBlocks(standalone.sceneXml,
                                    "SomeOtherApplication") == 1);
  const ConfigManager::DirtyState dirtyBeforeSnapshot =
      config.CaptureDirtyState();
  const MvrScene sceneBeforeSnapshot = scene;
  std::vector<uint8_t> snapshotBytes;
  assert(exporter.ExportCanonicalSnapshotToBuffer(scene, snapshotBytes));
  assert(!snapshotBytes.empty());
  const ConfigManager::DirtyState dirtyAfterSnapshot =
      config.CaptureDirtyState();
  assert(dirtyAfterSnapshot.revision == dirtyBeforeSnapshot.revision);
  assert(dirtyAfterSnapshot.savedRevision == dirtyBeforeSnapshot.savedRevision);
  assert(scene.fixtures.size() == sceneBeforeSnapshot.fixtures.size());
  for (const auto &[uuid, fixture] : sceneBeforeSnapshot.fixtures) {
    assert(scene.fixtures.at(uuid).uuid == fixture.uuid);
    assert(scene.fixtures.at(uuid).transform.u == fixture.transform.u);
    assert(scene.fixtures.at(uuid).transform.v == fixture.transform.v);
    assert(scene.fixtures.at(uuid).transform.w == fixture.transform.w);
    assert(scene.fixtures.at(uuid).transform.o == fixture.transform.o);
  }
  for (const auto &[uuid, expected] : editableIds) {
    const Fixture &fixture = scene.fixtures.at(uuid);
    assert(fixture.fixtureId == expected.first);
    assert(fixture.fixtureIdText == expected.second);
  }

  config.Reset();
  assert(MvrImporter::ImportAndRegister(standalonePath.string(), false,
                                        false));
  AssertRestoredScene(config.GetScene());
  assert(config.GetScene().opaqueUserDataBlocks.size() == 1);

  MvrExportOptions projectOptions = CanonicalMvrExportOptions();
  const fs::path explicitProjectMvr = workspace.Path() / "project-option.mvr";
  assert(exporter.ExportToFile(explicitProjectMvr.string(), projectOptions));
  const ArchiveSnapshot explicitProject = ReadMvr(explicitProjectMvr);
  const SceneSignature explicitSignature =
      ParseSceneSignature(explicitProject.sceneXml, true);
  assert(standaloneSignature.metadata == explicitSignature.metadata);
  assert(standaloneSignature.idsByFixtureName ==
         explicitSignature.idsByFixtureName);

  const std::string caseAReference =
      explicitSignature.gdtfByFixtureName.at("Fixture A");
  const std::string caseBReference =
      explicitSignature.gdtfByFixtureName.at("Case B");
  assert(!caseAReference.empty() && !caseBReference.empty());
  assert(caseAReference != caseBReference);
  assert(FoldAscii(caseAReference) != FoldAscii(caseBReference));
  assert(caseAReference.find_first_of("/\\:") == std::string::npos);
  assert(caseBReference.find_first_of("/\\:") == std::string::npos);
  assert(explicitProject.entries.count(caseAReference) == 1);
  assert(explicitProject.entries.count(caseBReference) == 1);
  assert(ReadGdtfIdentity(explicitProject.entries.at(caseAReference)) ==
         std::make_pair(std::string("Case A"),
                        std::string("cccccccc-cccc-4ccc-8ccc-ccccccccccc3")));
  assert(ReadGdtfIdentity(explicitProject.entries.at(caseBReference)) ==
         std::make_pair(std::string("Case B"),
                        std::string("dddddddd-dddd-4ddd-8ddd-ddddddddddd4")));

  const fs::path firstProject = workspace.Path() / "first.pera";
  assert(config.SaveProject(firstProject.string()));
  const SceneSignature firstSignature =
      ParseSceneSignature(ReadProjectMvr(firstProject).sceneXml, true);
  const ArchiveSnapshot firstProjectMvr = ReadProjectMvr(firstProject);
  assert(standalone.entries.size() == firstProjectMvr.entries.size());
  for (const auto &[entryName, standaloneBytes] : standalone.entries) {
    assert(firstProjectMvr.entries.count(entryName) == 1);
    if (entryName != "GeneralSceneDescription.xml")
      AssertCanonicalResourcePayloadEqual(
          entryName, standaloneBytes, firstProjectMvr.entries.at(entryName));
  }
  assert(CountForeignProviderBlocks(firstProjectMvr.sceneXml,
                                    "SomeOtherApplication") == 1);
  config.Reset();
  assert(config.LoadProject(firstProject.string()));
  AssertRestoredScene(config.GetScene());

  const fs::path secondProject = workspace.Path() / "second.pera";
  assert(config.SaveProject(secondProject.string()));
  const ArchiveSnapshot secondProjectMvr = ReadProjectMvr(secondProject);
  const SceneSignature secondSignature =
      ParseSceneSignature(secondProjectMvr.sceneXml, true);
  assert(secondSignature.metadata == firstSignature.metadata);
  assert(secondSignature.idsByFixtureName == firstSignature.idsByFixtureName);
  assert(secondSignature.gdtfByFixtureName.at("Fixture A") == caseAReference);
  assert(secondSignature.gdtfByFixtureName.at("Case B") == caseBReference);
  assert(CountForeignProviderBlocks(secondProjectMvr.sceneXml,
                                    "SomeOtherApplication") == 1);
  config.Reset();
  assert(config.LoadProject(secondProject.string()));
  AssertRestoredScene(config.GetScene());
  return 0;
}
