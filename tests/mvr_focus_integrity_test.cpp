/* This file is part of Perastage. */
#include "configmanager.h"
#include "inspection/xml_schema_validation.h"
#include "matrixutils.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvrscenemerger.h"
#include "mvr_standard_preservation.h"
#include "scene_grouping.h"
#include "scene_clipboard.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"

#include <tinyxml2.h>
#include <wx/init.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace perastage::inspection;
constexpr auto layerId = "10000000-0000-4000-8000-000000000001";
constexpr auto groupId = "20000000-0000-4000-8000-000000000001";
constexpr auto focusId = "30000000-0000-4000-8000-000000000001";
constexpr auto secondFocusId = "30000000-0000-4000-8000-000000000002";

static std::string Wrap(const std::string &children, const std::string &aux = {}) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"IntegrityFixture\" providerVersion=\"1\">"
      "<UserData><Data provider=\"Foreign\" ver=\"1\"><Keep value=\"42\"/></Data></UserData>"
      "<Scene><Layers><Layer uuid=\"" + std::string(layerId) + "\" name=\"Rig\"><ChildList>" +
      children + "</ChildList></Layer></Layers><AUXData><Position uuid=\"50000000-0000-4000-8000-000000000001\" name=\"Front\"/>" + aux +
      "</AUXData></Scene></GeneralSceneDescription>";
}

static std::string FixtureXml(int id, const std::string &extra = {}) {
  return "<Fixture uuid=\"40000000-0000-4000-8000-00000000000" + std::to_string(id) +
      "\" name=\"Fixture " + std::to_string(id) + "\"><GDTFSpec>fixture.gdtf</GDTFSpec>"
      "<GDTFMode>Mode</GDTFMode><Focus>" + focusId + "</Focus>"
      "<DMXInvertPan>true</DMXInvertPan><DMXInvertTilt>true</DMXInvertTilt>"
      "<Position>50000000-0000-4000-8000-000000000001</Position><Function>Key light</Function><FixtureID>" + std::to_string(id) +
      "</FixtureID><FixtureIDNumeric>" + std::to_string(id) + "</FixtureIDNumeric>"
      "<UnitNumber>" + std::to_string(id) + "</UnitNumber>"
      "<Addresses><Address break=\"0\">513</Address></Addresses>"
      "<Color>0.314303,0.328065,87.699166</Color>"
      "<CustomIdType>2</CustomIdType><CustomId>45</CustomId>" + extra + "</Fixture>";
}

static std::string FocusXml(const char *id) {
  return "<FocusPoint uuid=\"" + std::string(id) + "\" name=\"Aim\">"
      "<Matrix>{0,1,0}{-1,0,0}{0,0,1}{100,200,300}</Matrix>"
      "<Geometries><Geometry3D fileName=\"target.glb\">"
      "<Matrix>{1,0,0}{0,1,0}{0,0,1}{7,8,9}</Matrix>"
      "</Geometry3D></Geometries></FocusPoint>";
}

static void WriteSource(const fs::path &dir, const std::string &xml) {
  const auto gdtf = tests::archive::ReadBinaryFile((dir / "fixture.gdtf").string());
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(dir / "source.mvr",
      {{"GeneralSceneDescription.xml", xml},
       {"target.glb", tests::gdtf::BuildMinimalValidGlb()},
       {"fixture.gdtf", std::string(gdtf.begin(), gdtf.end())}}, error));
}

static MvrScene Import(const fs::path &file) {
  MvrImporter importer;
  MvrImportResult result;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  assert(importer.ImportFromFile(file.string(), result, MvrImportMode::ParseOnly, options));
  return std::move(result.scene);
}

static void CheckSchema(const std::string &xml) {
  const auto full = ValidateXmlAgainstSchema(xml, Mvr16Schema());
  assert(full.xml.status == ValidationStatus::Valid);
  // The unmodified official XSD defines Data as empty although the published
  // format permits provider content. Record that disagreement explicitly.
  assert(full.schema.status == ValidationStatus::Invalid);
  tinyxml2::XMLDocument standard;
  assert(standard.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  standard.RootElement()->DeleteChild(standard.RootElement()->FirstChildElement("UserData"));
  tinyxml2::XMLPrinter printer;
  standard.Print(&printer);
  const auto structure = ValidateXmlAgainstSchema(printer.CStr(), Mvr16Schema());
  if (structure.schema.status != ValidationStatus::Valid)
    for (const auto &diagnostic : structure.schema.diagnostics)
      std::cerr << diagnostic.message << '\n';
  assert(structure.schema.status == ValidationStatus::Valid);
}

static void CheckExport(const fs::path &file, std::size_t focusCount = 2,
                        std::size_t fixtureCount = 2) {
  std::string xml;
  assert(tests::zip::ReadEntry(file, "GeneralSceneDescription.xml", &xml));
  CheckSchema(xml);
  tinyxml2::XMLDocument document;
  assert(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  std::unordered_set<std::string> focusUuids;
  std::vector<tinyxml2::XMLElement *> fixtures;
  const auto visit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
    if (std::string(node->Name()) == "FocusPoint") {
      focusUuids.insert(node->Attribute("uuid"));
      assert(node->FirstChildElement("Geometries"));
      assert(!node->FirstChildElement("FixtureID"));
    }
    if (std::string(node->Name()) == "Fixture") fixtures.push_back(node);
    for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement())
      self(self, child);
  };
  visit(visit, document.RootElement());
  assert(focusUuids.size() == focusCount);
  assert(fixtures.size() == fixtureCount);
  for (auto *fixture : fixtures) {
    assert(focusUuids.contains(fixture->FirstChildElement("Focus")->GetText()));
    assert(std::string(fixture->FirstChildElement("Function")->GetText()) == "Key light");
    assert(std::string(fixture->FirstChildElement("DMXInvertPan")->GetText()) == "true");
    assert(std::string(fixture->FirstChildElement("DMXInvertTilt")->GetText()) == "true");
    assert(std::string(fixture->FirstChildElement("Color")->GetText()) == "0.314303,0.328065,87.699166");
    assert(fixture->FirstChildElement("CustomId")->IntText() == 45);
  }
  assert(xml.find("provider=\"Foreign\"") != std::string::npos);
}

int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path dir = argc > 1 ? fs::path(argv[1]) :
      fs::temp_directory_path() / "perastage_mvr_focus_integrity";
  fs::create_directories(dir);
  tests::gdtf::BuildMinimalValidFixture().WithDmxMode("Mode", "Root").WriteArchive(dir / "fixture.gdtf");
  const std::string sourceXml = Wrap("<GroupObject uuid=\"" + std::string(groupId) +
      "\" name=\"Nested\"><Matrix>{1,0,0}{0,1,0}{0,0,1}{1000,2000,3000}</Matrix>"
      "<ChildList><GroupObject uuid=\"20000000-0000-4000-8000-000000000002\" name=\"Inner\">"
      "<Matrix>{1,0,0}{0,1,0}{0,0,1}{10,20,30}</Matrix><ChildList>" + FocusXml(focusId) +
      "</ChildList></GroupObject>" + FixtureXml(1) + FixtureXml(2) +
      "</ChildList></GroupObject>" + FocusXml(secondFocusId));
  CheckSchema(sourceXml);
  WriteSource(dir, sourceXml);
  MvrScene scene = Import(dir / "source.mvr");
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "unchanged.mvr").string()));
  CheckExport(dir / "unchanged.mvr");
  // This first export check also runs against the pre-fix serializer.
  if (argc > 2 && std::string(argv[2]) == "regression") return 0;
  assert(scene.preservationIssues.empty());
  assert(scene.sceneObjects.at(focusId).kind == SceneObjectKind::FocusPoint);
  assert(scene.sceneObjects.at(focusId).transform.o[0] == 1110);
  scene.fixtures.begin()->second.instanceName = "Edited fixture";
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "edited.mvr").string()));
  CheckExport(dir / "edited.mvr");
  auto &config = ConfigManager::Get();
  config.Reset();
  config.GetScene() = scene;
  assert(config.SaveProject((dir / "roundtrip.pstg").string()));
  const auto originalExtraction = scene.basePath;
  scene.Clear();
  config.Reset();
  assert(!fs::exists(originalExtraction));
  assert(config.LoadProject((dir / "roundtrip.pstg").string()));
  assert(exporter.ExportCanonicalSnapshotToFile(config.GetScene(), (dir / "reopened.mvr").string()));
  CheckExport(dir / "reopened.mvr");
  MvrScene reimported = Import(dir / "reopened.mvr");
  assert(exporter.ExportCanonicalSnapshotToFile(reimported, (dir / "repeated.mvr").string()));
  CheckExport(dir / "repeated.mvr");
  scene = Import(dir / "reopened.mvr");
  const auto merged = mvr::MergeImportedSceneIntoCurrent(scene, reimported);
  assert(merged.sceneObjectsAdded == 2);
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "merged.mvr").string()));
  CheckExport(dir / "merged.mvr", 4, 4);
  scene_grouping::TranslateTargets(scene, {{MvrNodeType::GroupObject, groupId}}, {5, 6, 7});
  assert(scene.sceneObjects.at(focusId).transform.o[0] == 1115);
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "transformed.mvr").string()));
  CheckExport(dir / "transformed.mvr", 4, 4);
  MvrScene copied = Import(dir / "reopened.mvr");
  SelectionState selected;
  const auto fixtureUuid = copied.fixtures.begin()->first;
  selected.SetSelectedFixtures({fixtureUuid});
  selected.SetSelectedSceneObjects({focusId});
  scene_clipboard::Service clipboard;
  assert(clipboard.Capture(copied, selected, 1));
  const auto paste = clipboard.Paste(copied, 1);
  assert(paste.changed);
  assert(copied.fixtures.at(paste.uuidRemap.at(fixtureUuid)).focus == paste.uuidRemap.at(focusId));
  assert(copied.sceneObjects.at(paste.uuidRemap.at(focusId)).kind == SceneObjectKind::FocusPoint);
  assert(exporter.ExportCanonicalSnapshotToFile(copied, (dir / "copied.mvr").string()));
  CheckExport(dir / "copied.mvr", 3, 3);
  scene_grouping::ObjectSelection grouping;
  grouping.sceneObjects = {secondFocusId, paste.uuidRemap.at(focusId)};
  const auto group = scene_grouping::GroupSelection(copied, grouping);
  assert(group.changed);
  assert(exporter.ExportCanonicalSnapshotToFile(copied, (dir / "grouped.mvr").string()));
  CheckExport(dir / "grouped.mvr", 3, 3);
  // A dangling target must fail before recovery can clear the reference.
  MvrScene dangling = reimported;
  dangling.sceneObjects.erase(focusId);
  std::vector<uint8_t> bytes{1, 2, 3};
  assert(!exporter.ExportCanonicalSnapshotToBuffer(dangling, bytes));
  assert(bytes.empty());
  assert(exporter.GetExportDiagnostics().front().code ==
         MvrExportDiagnosticCode::StandardDataPreservationBlocked);
  MvrScene missing = reimported;
  missing.sceneObjects.at(focusId).geometries.front().modelFile = "absent.glb";
  assert(!exporter.ExportCanonicalSnapshotToBuffer(missing, bytes));
  std::string missingSource = Wrap(FocusXml(focusId));
  missingSource.replace(missingSource.find("target.glb"), 10, "absent.glb");
  WriteSource(dir, missingSource);
  assert(!exporter.ExportCanonicalSnapshotToBuffer(Import(dir / "source.mvr"), bytes));
  for (const auto &extra : {"<Alignments><Alignment geometry=\"Beam\" up=\"0,0,1\" direction=\"0,0,-1\"/></Alignments>",
                             "<Protocols><Protocol type=\"Art-Net\"/></Protocols>"}) {
    WriteSource(dir, Wrap(FocusXml(focusId) + FixtureXml(1, extra)));
    MvrScene unsafe = Import(dir / "source.mvr");
    assert(!unsafe.preservationIssues.empty());
    assert(!unsafe.preservationIssues.front().originalInformation.empty());
    assert(!exporter.ExportCanonicalSnapshotToBuffer(unsafe, bytes));
    config.GetScene() = unsafe;
    assert(!config.SaveProject((dir / "roundtrip.pstg").string()));
    assert(config.LoadProject((dir / "roundtrip.pstg").string()));
    assert(config.GetScene().sceneObjects.size() == 2);
    mvr::MergeImportedSceneIntoCurrent(reimported, unsafe);
    assert(!exporter.ExportCanonicalSnapshotToBuffer(reimported, bytes));
  }
  MvrScene future;
  future.versionMinor = 7;
  assert(!exporter.ExportCanonicalSnapshotToBuffer(future, bytes));
  std::string futureSource = sourceXml;
  futureSource.replace(futureSource.find("verMinor=\"6\""), 12, "verMinor=\"7\"");
  WriteSource(dir, futureSource);
  assert(!exporter.ExportCanonicalSnapshotToBuffer(Import(dir / "source.mvr"), bytes));
  WriteSource(dir, Wrap("<FocusPoint uuid=\"" + std::string(focusId) + "\"><Geometries/></FocusPoint>"));
  assert(exporter.ExportCanonicalSnapshotToFile(Import(dir / "source.mvr"), (dir / "empty.mvr").string()));
  CheckExport(dir / "empty.mvr", 1, 0);
  std::string legacy = Wrap(FocusXml(focusId) + FocusXml(secondFocusId));
  legacy.replace(legacy.find("verMinor=\"6\""), 12, "verMinor=\"5\"");
  WriteSource(dir, legacy);
  assert(exporter.ExportCanonicalSnapshotToFile(Import(dir / "source.mvr"), (dir / "legacy.mvr").string()));
  CheckExport(dir / "legacy.mvr", 2, 0);
  WriteSource(dir, sourceXml);
  std::cout << "Focus preservation, PSTG, merge and loss protection passed\n";
}
