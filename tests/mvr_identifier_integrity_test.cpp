/* This file is part of Perastage. */
#include "configmanager.h"
#include "inspection/xml_schema_validation.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvrscenemerger.h"
#include "mvr_identifier_preservation.h"
#include "mvr_export_preparation.h"
#include "scene_clipboard.h"
#include "scene_grouping.h"
#include "truss_instance_replacement.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include <wx/init.h>
#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
constexpr auto fixtureId = "40000000-0000-4000-8000-000000000001";
constexpr auto secondId = "40000000-0000-4000-8000-000000000002";

static std::string Source() {
  const std::string geometry = "<Geometries><Geometry3D fileName='model.glb'/></Geometries>";
  std::string fixtures;
  for (const auto &id : {fixtureId, secondId})
    fixtures += "<Fixture uuid='" + std::string(id) + "' name='Light'>"
        "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode>"
        "<FixtureID>" + (id == fixtureId ? std::string("  House &amp; 1  ") : "101") +
        "</FixtureID><FixtureIDNumeric>0</FixtureIDNumeric><UnitNumber>0</UnitNumber></Fixture>";
  return "<GeneralSceneDescription verMajor='1' verMinor='6' provider='IdentifierTest' providerVersion='1'>"
      "<Scene><Layers><Layer uuid='10000000-0000-4000-8000-000000000001' name='Rig'><ChildList>" + fixtures +
      "<Truss uuid='70000000-0000-4000-8000-000000000001' name='Different display name'>" + geometry +
      "<FixtureID>Truss tag</FixtureID><FixtureIDNumeric>0</FixtureIDNumeric></Truss>"
      "<Support uuid='80000000-0000-4000-8000-000000000001'>" + geometry +
      "<ChainLength>1000</ChainLength><FixtureID>Hoist tag</FixtureID><FixtureIDNumeric>0</FixtureIDNumeric></Support>"
      "<SceneObject uuid='60000000-0000-4000-8000-000000000001'>" + geometry +
      "<FixtureID/><FixtureIDNumeric>0</FixtureIDNumeric></SceneObject>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
}

static MvrScene Import(const fs::path &path) {
  MvrImporter importer;
  MvrImportResult result;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  assert(importer.ImportFromFile(path.string(), result, MvrImportMode::ParseOnly, options));
  return std::move(result.scene);
}

static void WriteSource(const fs::path &dir, const std::string &xml) {
  const auto gdtf = tests::archive::ReadBinaryFile((dir / "fixture.gdtf").string());
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(dir / "source.mvr",
      {{"GeneralSceneDescription.xml", xml}, {"model.glb", tests::gdtf::BuildMinimalValidGlb()},
       {"fixture.gdtf", std::string(gdtf.begin(), gdtf.end())}}, error));
}

int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "perastage_mvr_identifier_integrity";
  fs::create_directories(dir);
  tests::gdtf::BuildMinimalValidFixture().WithDmxMode("Mode", "Root").WriteArchive(dir / "fixture.gdtf");
  const auto xml = Source();
  using namespace perastage::inspection;
  assert(ValidateXmlAgainstSchema(xml, Mvr16Schema()).schema.status == ValidationStatus::Valid);
  WriteSource(dir, xml);
  auto scene = Import(dir / "source.mvr");
  assert(scene.fixtures.at(secondId).fixtureIdNumeric == 0);
  assert(scene.fixtures.at(fixtureId).fixtureIdText == "  House & 1  ");
  assert(scene.preservationIssues.empty());
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "unchanged.mvr").string()));
  auto &config = ConfigManager::Get();
  config.Reset();
  config.GetScene() = scene;
  assert(exporter.ExportToFile((dir / "canonical.mvr").string()));
  assert(config.SaveProject((dir / "roundtrip.pstg").string()));
  const auto extraction = scene.basePath;
  scene.Clear();
  config.Reset();
  assert(!fs::exists(extraction));
  assert(fs::remove(dir / "source.mvr"));
  assert(config.LoadProject((dir / "roundtrip.pstg").string()));
  assert(exporter.ExportToFile((dir / "second-canonical.mvr").string()));
  assert(config.SaveProject((dir / "second-cycle.pstg").string()));
  scene = config.GetScene();
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "reopened.mvr").string()));
  auto incoming = Import(dir / "reopened.mvr");
  auto &truss = scene.trusses.at("70000000-0000-4000-8000-000000000001");
  auto replacement = truss;
  replacement.fixtureIdText = "Unrelated type label";
  replacement.fixtureIdNumeric = 999;
  replacement.identifierSource.reset();
  replacement.unitNumber = 999;
  replacement.classing = "a0000000-0000-4000-8000-000000000001";
  replacement.model = "Replacement type";
  truss_instance::ReplaceType(truss, replacement);
  assert(truss.fixtureIdText == "Truss tag" && truss.fixtureIdNumeric == 0);
  assert(truss.identifierSource && !truss.identifierSource->unitNumberPresent);
  assert(truss.unitNumber == 0 && truss.model == "Replacement type");
  assert(truss.classing.empty());
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "replaced.mvr").string()));
  const auto merge = mvr::AnalyzeImportedSceneMerge(scene, incoming);
  mvr::ApplyImportedSceneMergeAtomically(scene, incoming, merge);
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "merged.mvr").string()));
  SelectionState selection;
  selection.SetSelectedFixtures({fixtureId});
  scene_clipboard::Service clipboard;
  assert(clipboard.Capture(scene, selection, 1));
  assert(clipboard.Paste(scene, 1).changed);
  scene_grouping::ObjectSelection group;
  group.fixtures = {fixtureId, secondId};
  assert(scene_grouping::GroupSelection(scene, group).changed);
  scene_grouping::TranslateTargets(scene, {{MvrNodeType::Fixture, fixtureId}}, {5, 6, 7});
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "operations.mvr").string()));
  scene.fixtures.at(fixtureId).fixtureId = 42;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "edited.mvr").string()));
  scene.fixtures.at(fixtureId).fixtureId = 0;
  scene.fixtures.at(fixtureId).fixtureIdNumeric = 63;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "numeric-edited.mvr").string()));
  scene.supports.at("80000000-0000-4000-8000-000000000001").unitNumber = 5;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "unit-edited.mvr").string()));
  const auto replaceAll = [](std::string text, const std::string &from, const std::string &to) {
    std::size_t at = 0;
    while ((at = text.find(from, at)) != std::string::npos) {
      text.replace(at, from.size(), to);
      at += to.size();
    }
    return text;
  };
  auto units = replaceAll(xml, "</FixtureIDNumeric></Truss>", "</FixtureIDNumeric><UnitNumber>0</UnitNumber></Truss>");
  units = replaceAll(units, "</FixtureIDNumeric></Support>", "</FixtureIDNumeric><UnitNumber>0</UnitNumber></Support>");
  units = replaceAll(units, "</FixtureIDNumeric></SceneObject>", "</FixtureIDNumeric><UnitNumber>7</UnitNumber></SceneObject>");
  auto stale = xml;
  stale.insert(stale.find("<Scene>"), "<UserData><Data provider='Perastage' ver='1'>"
      "<ProjectFixtureMetadataMap schemaVersion='1.0'><ProjectFixtureMetadata uuid='" + std::string(fixtureId) +
      "' fixtureId='93' fixtureIdNumeric='999' fixtureIdText='stale' unitNumber='88'/>"
      "</ProjectFixtureMetadataMap></Data></UserData>");
  for (const auto &[name, variant] : std::vector<std::pair<std::string, std::string>>{
           {"units", units}, {"stale", stale},
           {"empty", replaceAll(replaceAll(xml, "<FixtureID>  House &amp; 1  </FixtureID>", "<FixtureID/>"),
                                "<FixtureID>101</FixtureID>", "<FixtureID/>")},
           {"positive", replaceAll(replaceAll(xml, "Numeric>0", "Numeric>7"), "Number>0", "Number>+0007")},
           {"negative", replaceAll(replaceAll(xml, "Numeric>0", "Numeric>-6"), "Number>0", "Number>-6")}}) {
    if (name == "units" || name == "positive" || name == "empty")
      assert(ValidateXmlAgainstSchema(variant, Mvr16Schema()).schema.status == ValidationStatus::Valid);
    if (name == "negative")
      assert(ValidateXmlAgainstSchema(variant, Mvr16Schema()).schema.status == ValidationStatus::Invalid);
    WriteSource(dir, variant);
    fs::copy_file(dir / "source.mvr", dir / (name + "-source.mvr"), fs::copy_options::overwrite_existing);
    auto authored = Import(dir / "source.mvr");
    assert(authored.preservationIssues.empty());
    assert(exporter.ExportCanonicalSnapshotToFile(authored, (dir / (name + ".mvr")).string()));
    config.GetScene() = authored;
    assert(config.SaveProject((dir / (name + ".pstg")).string()));
    config.Reset();
    assert(config.LoadProject((dir / (name + ".pstg")).string()));
    assert(exporter.ExportToFile((dir / (name + "-reopened.mvr")).string()));
  }
  const auto previous = tests::archive::ReadBinaryFile((dir / "roundtrip.pstg").string());
  for (const auto &number : {"2147483648", "-2147483649", "12junk", "+-1"}) {
    WriteSource(dir, replaceAll(xml, "Numeric>0", std::string("Numeric>") + number));
    config.GetScene() = Import(dir / "source.mvr");
    assert(!config.GetScene().preservationIssues.empty());
    if (std::string(number) == "2147483648") {
      auto target = Import(dir / "canonical.mvr");
      mvr::MergeImportedSceneIntoCurrent(target, config.GetScene());
      std::vector<uint8_t> mergedBytes{1};
      assert(!exporter.ExportCanonicalSnapshotToBuffer(target, mergedBytes));
      assert(mergedBytes.empty());
    }
    std::vector<uint8_t> rejected{1, 2};
    assert(!exporter.ExportToBuffer(rejected));
    assert(rejected.empty());
    assert(!config.SaveProject((dir / "roundtrip.pstg").string()));
    assert(config.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::MvrPreservation);
    assert(tests::archive::ReadBinaryFile((dir / "roundtrip.pstg").string()) == previous);
    assert(!fs::exists((dir / "roundtrip.pstg").string() + ".tmp"));
  }
  for (const auto &broken : {
      replaceAll(xml, "<UnitNumber>0</UnitNumber>", "<UnitNumber>0</UnitNumber><UnitNumber>1</UnitNumber>"),
      replaceAll(xml, "<FixtureID/>", "<FixtureID><Nested/>text</FixtureID>"),
      replaceAll(xml, "<UnitNumber>0</UnitNumber>", "<UnitNumber>2147483648</UnitNumber>")}) {
    WriteSource(dir, broken);
    auto protectedScene = Import(dir / "source.mvr");
    std::vector<uint8_t> rejected{1};
    assert(!protectedScene.preservationIssues.empty());
    assert(!exporter.ExportCanonicalSnapshotToBuffer(protectedScene, rejected));
    assert(rejected.empty());
  }
  WriteSource(dir, xml);
  const auto prepared = mvr_export_preparation::Prepare(Import(dir / "source.mvr"), CanonicalMvrExportOptions());
  std::string exported;
  assert(tests::zip::ReadEntry(dir / "unchanged.mvr", "GeneralSceneDescription.xml", &exported));
  tinyxml2::XMLDocument document;
  assert(document.Parse(exported.c_str()) == tinyxml2::XML_SUCCESS);
  const auto verify = [&] { return mvr_identifiers::CheckProjection(*document.RootElement(),
      prepared.scene, prepared.objectIds, prepared.fixtureUnitNumbers); };
  assert(verify().empty());
  const auto findFixture = [&](auto &&self, tinyxml2::XMLElement *node) -> tinyxml2::XMLElement * {
    if (std::string(node->Name()) == "Fixture") return node;
    for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement())
      if (auto *found = self(self, child)) return found;
    return nullptr;
  };
  auto *fixture = findFixture(findFixture, document.RootElement()->FirstChildElement("Scene"));
  assert(fixture);
  fixture->FirstChildElement("FixtureIDNumeric")->SetText(123);
  assert(!verify().empty());
  std::cout << "Identifier preservation and source ownership passed\n";
}
