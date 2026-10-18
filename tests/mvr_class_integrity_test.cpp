/* This file is part of Perastage. */
#include "configmanager.h"
#include "inspection/xml_schema_validation.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvrscenemerger.h"
#include "scene_clipboard.h"
#include "scene_grouping.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include <wx/init.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>

namespace fs = std::filesystem;
using namespace perastage::inspection;
constexpr auto classId = "a0000000-0000-4000-8000-000000000001";
constexpr auto unusedId = "a0000000-0000-4000-8000-000000000002";
constexpr auto emptyId = "a0000000-0000-4000-8000-000000000003";
constexpr auto fixtureId = "40000000-0000-4000-8000-000000000001";
constexpr auto focusId = "30000000-0000-4000-8000-000000000001";

static std::string Source() {
  const std::string classing = "<Classing>" + std::string(classId) + "</Classing>";
  const std::string geometry = "<Geometries><Geometry3D fileName='target.glb'/></Geometries>";
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor='1' verMinor='6' provider='IntegrityFixture' providerVersion='1'><Scene><Layers>"
      "<Layer uuid='10000000-0000-4000-8000-000000000001' name='One'><ChildList>"
      "<GroupObject uuid='20000000-0000-4000-8000-000000000001'>" + classing + "<ChildList>"
      "<Fixture uuid='" + fixtureId + "'>" + classing +
      "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode><Focus>" + focusId +
      "</Focus><FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric><UnitNumber>1</UnitNumber>"
      "<Addresses><Address>1</Address></Addresses></Fixture>"
      "<FocusPoint uuid='" + focusId + "'>" + classing + geometry + "</FocusPoint>"
      "<SceneObject uuid='60000000-0000-4000-8000-000000000001'>" + classing + geometry +
      "<FixtureID>2</FixtureID><FixtureIDNumeric>2</FixtureIDNumeric></SceneObject>"
      "</ChildList></GroupObject></ChildList></Layer>"
      "<Layer uuid='10000000-0000-4000-8000-000000000002' name='Two'><ChildList>"
      "<Truss uuid='70000000-0000-4000-8000-000000000001'>" + classing + geometry +
      "<FixtureID>3</FixtureID><FixtureIDNumeric>3</FixtureIDNumeric></Truss>"
      "<Support uuid='80000000-0000-4000-8000-000000000001'>" + classing + geometry +
      "<ChainLength>1000</ChainLength><FixtureID>4</FixtureID><FixtureIDNumeric>4</FixtureIDNumeric></Support>"
      "</ChildList></Layer></Layers><AUXData><Class uuid='" + classId +
      "' name='  Shared &amp; exact  '/><Class uuid='" + unusedId + "'/><Class uuid='" + emptyId +
      "' name=''/></AUXData></Scene></GeneralSceneDescription>";
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

static void CheckSchema(const std::string &xml, bool exported) {
  const auto full = ValidateXmlAgainstSchema(xml, Mvr16Schema());
  assert(full.xml.status == ValidationStatus::Valid);
  if (!exported) {
    for (const auto &diagnostic : full.schema.diagnostics)
      std::cerr << diagnostic.message << '\n';
    assert(full.schema.status == ValidationStatus::Valid);
    return;
  }
  // Published provider payloads disagree with the unchanged official XSD.
  assert(full.schema.status == ValidationStatus::Invalid);
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  doc.RootElement()->DeleteChild(doc.RootElement()->FirstChildElement("UserData"));
  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  const auto standard = ValidateXmlAgainstSchema(printer.CStr(), Mvr16Schema());
  for (const auto &diagnostic : standard.schema.diagnostics)
    std::cerr << diagnostic.message << '\n';
  assert(standard.schema.status == ValidationStatus::Valid);
}

static void CheckExport(const fs::path &file, int instances = 6, int classes = 3) {
  std::string xml;
  assert(tests::zip::ReadEntry(file, "GeneralSceneDescription.xml", &xml));
  CheckSchema(xml, true);
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  auto *scene = doc.RootElement()->FirstChildElement("Scene");
  auto *aux = scene->FirstChildElement("AUXData");
  assert(aux);
  std::map<std::string, std::string> names;
  int definitions = 0;
  for (auto *item = aux->FirstChildElement("Class"); item; item = item->NextSiblingElement("Class")) {
    ++definitions;
    names[item->Attribute("uuid")] = item->Attribute("name") ? item->Attribute("name") : "<absent>";
  }
  assert(definitions == classes);
  assert(names.at(classId) == "  Shared & exact  ");
  assert(names.at(unusedId) == "<absent>");
  assert(names.at(emptyId).empty());
  int references = 0;
  const auto visit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
    if (std::string(node->Name()) == "Classing") {
      ++references;
      assert(node->GetText() && names.contains(node->GetText()));
    }
    for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement())
      self(self, child);
  };
  visit(visit, scene->FirstChildElement("Layers"));
  assert(references == instances);
  // Geometry ownership remains in the archive after the import lease changes.
  const auto imported = Import(file);
  for (const auto &[uuid, object] : imported.sceneObjects)
    for (const auto &geometry : object.geometries) {
      const auto bytes = tests::archive::ReadBinaryFile((fs::path(imported.basePath) / geometry.modelFile).string());
      assert(std::string(bytes.begin(), bytes.end()) == tests::gdtf::BuildMinimalValidGlb());
    }
}

int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "perastage_mvr_class_integrity";
  fs::create_directories(dir);
  tests::gdtf::BuildMinimalValidFixture().WithDmxMode("Mode", "Root").WriteArchive(dir / "fixture.gdtf");
  const auto gdtf = tests::archive::ReadBinaryFile((dir / "fixture.gdtf").string());
  const auto xml = Source();
  CheckSchema(xml, false);
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(dir / "source.mvr",
      {{"GeneralSceneDescription.xml", xml}, {"target.glb", tests::gdtf::BuildMinimalValidGlb()},
       {"fixture.gdtf", std::string(gdtf.begin(), gdtf.end())}}, error));
  MvrScene scene = Import(dir / "source.mvr");
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "unchanged.mvr").string()));
  CheckExport(dir / "unchanged.mvr");
  assert(scene.preservationIssues.empty());
  assert(scene.classes.size() == 3);
  assert(scene.fixtures.at(fixtureId).classing == classId);
  assert(scene.sceneObjects.at(focusId).classing == classId);
  scene.fixtures.at(fixtureId).function = "Unrelated edit";
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "edited.mvr").string()));
  CheckExport(dir / "edited.mvr");
  auto &config = ConfigManager::Get();
  config.Reset();
  config.GetScene() = scene;
  assert(config.SaveProject((dir / "roundtrip.pstg").string()));
  const auto extraction = scene.basePath;
  scene.Clear();
  assert(scene.classes.empty());
  config.Reset();
  assert(!fs::exists(extraction));
  assert(config.LoadProject((dir / "roundtrip.pstg").string()));
  scene = config.GetScene();
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "reopened.mvr").string()));
  CheckExport(dir / "reopened.mvr");
  const MvrScene incoming = Import(dir / "reopened.mvr");
  assert(exporter.ExportCanonicalSnapshotToFile(incoming, (dir / "repeated.mvr").string()));
  CheckExport(dir / "repeated.mvr");

  MvrScene shared = scene;
  mvr::MergeImportedSceneIntoCurrent(shared, incoming);
  assert(shared.classes.size() == 3);
  assert(exporter.ExportCanonicalSnapshotToFile(shared, (dir / "shared.mvr").string()));
  CheckExport(dir / "shared.mvr", 12);

  MvrScene equivalent = incoming;
  equivalent.classes.at(unusedId).name = "";
  equivalent.classes.at(emptyId).name.reset();
  MvrScene defaultNames = incoming;
  const auto sameIdentity = mvr::AnalyzeImportedSceneMerge(defaultNames, equivalent);
  assert(sameIdentity.uuidMap.at(unusedId) == unusedId);
  assert(sameIdentity.uuidMap.at(emptyId) == emptyId);
  mvr::ApplyImportedSceneMergeAtomically(defaultNames, equivalent, sameIdentity);
  assert(exporter.ExportCanonicalSnapshotToFile(defaultNames, (dir / "equivalent.mvr").string()));
  CheckExport(dir / "equivalent.mvr", 12);

  MvrScene changed = incoming;
  changed.classes.at(classId).name = "Different incoming definition";
  MvrScene conflict = scene;
  const auto analysis = mvr::AnalyzeImportedSceneMerge(conflict, changed);
  const auto replacement = analysis.uuidMap.at(classId);
  assert(replacement != classId);
  mvr::ApplyImportedSceneMergeAtomically(conflict, changed, analysis);
  assert(conflict.classes.size() == 4);
  assert(conflict.classes.at(classId).name == "  Shared & exact  ");
  assert(conflict.classes.at(replacement).name == "Different incoming definition");
  assert(conflict.fixtures.at(analysis.uuidMap.at(fixtureId)).classing == replacement);
  assert(conflict.sceneObjects.at(analysis.uuidMap.at(focusId)).classing == replacement);
  assert(exporter.ExportCanonicalSnapshotToFile(conflict, (dir / "conflict.mvr").string()));
  CheckExport(dir / "conflict.mvr", 12, 4);

  // Equal names with different identities remain separate classes.
  MvrScene sameName;
  const std::string distinct = "a0000000-0000-4000-8000-000000000004";
  sameName.classes.emplace(distinct, incoming.classes.at(classId));
  mvr::MergeImportedSceneIntoCurrent(sameName, incoming);
  assert(sameName.classes.size() == 4);
  assert(sameName.fixtures.at(fixtureId).classing == classId);
  assert(exporter.ExportCanonicalSnapshotToFile(sameName, (dir / "same-name.mvr").string()));
  CheckExport(dir / "same-name.mvr", 6, 4);

  // A class collision with a different kind must never overwrite that owner,
  // including under ReplaceExisting or SkipIncoming object policies.
  for (const auto policy : {mvr::MvrMergeUuidCollisionBehavior::GenerateStableUuid,
                            mvr::MvrMergeUuidCollisionBehavior::ReplaceExisting,
                            mvr::MvrMergeUuidCollisionBehavior::SkipIncoming}) {
    MvrScene target;
    auto object = incoming.sceneObjects.at(focusId);
    object.uuid = classId;
    object.classing.clear();
    object.parentGroupUuid.clear();
    target.sceneObjects.emplace(classId, object);
    target.basePath = incoming.basePath;
    target.runtimeResourceLeases = incoming.runtimeResourceLeases;
    mvr::MvrMergeOptions options;
    options.uuidCollisionBehavior = policy;
    const auto remap = mvr::AnalyzeImportedSceneMerge(target, incoming, options);
    assert(remap.uuidMap.at(classId) != classId);
    mvr::ApplyImportedSceneMergeAtomically(target, incoming, remap);
    assert(target.sceneObjects.at(classId).classing.empty());
    assert(target.fixtures.at(fixtureId).classing == remap.uuidMap.at(classId));
    assert(target.classes.contains(remap.uuidMap.at(classId)));
    assert(exporter.ExportCanonicalSnapshotToFile(target, (dir / "cross-kind.mvr").string()));
  }
  for (const auto policy : {mvr::MvrMergeUuidCollisionBehavior::GenerateStableUuid,
                            mvr::MvrMergeUuidCollisionBehavior::ReplaceExisting,
                            mvr::MvrMergeUuidCollisionBehavior::SkipIncoming}) {
    MvrScene objectCollision;
    objectCollision.classes.emplace(fixtureId, MvrClass{"Reserved class"});
    mvr::MvrMergeOptions options;
    options.uuidCollisionBehavior = policy;
    const auto remap = mvr::AnalyzeImportedSceneMerge(objectCollision, incoming, options);
    assert(remap.uuidMap.at(fixtureId) != fixtureId);
    mvr::ApplyImportedSceneMergeAtomically(objectCollision, incoming, remap);
    assert(objectCollision.classes.contains(fixtureId));
    assert(objectCollision.fixtures.at(remap.uuidMap.at(fixtureId)).classing == classId);
    assert(exporter.ExportCanonicalSnapshotToFile(objectCollision, (dir / "object-collision.mvr").string()));
    CheckExport(dir / "object-collision.mvr", 6, 4);

  }

  SelectionState selected;
  selected.SetSelectedFixtures({fixtureId});
  selected.SetSelectedSceneObjects({focusId, "60000000-0000-4000-8000-000000000001"});
  selected.SetSelectedTrusses({"70000000-0000-4000-8000-000000000001"});
  selected.SetSelectedSupports({"80000000-0000-4000-8000-000000000001"});
  scene_clipboard::Service clipboard;
  assert(clipboard.Capture(scene, selected, 1));
  const auto pasted = clipboard.Paste(scene, 1);
  assert(pasted.changed);
  assert(scene.classes.size() == 3);
  assert(scene.fixtures.at(pasted.uuidRemap.at(fixtureId)).classing == classId);
  assert(scene.sceneObjects.at(pasted.uuidRemap.at(focusId)).classing == classId);
  scene_grouping::ObjectSelection selection;
  selection.fixtures = {pasted.uuidRemap.at(fixtureId)};
  selection.sceneObjects = {pasted.uuidRemap.at(focusId),
      pasted.uuidRemap.at("60000000-0000-4000-8000-000000000001")};
  selection.trusses = {pasted.uuidRemap.at("70000000-0000-4000-8000-000000000001")};
  selection.supports = {pasted.uuidRemap.at("80000000-0000-4000-8000-000000000001")};
  assert(scene_grouping::GroupSelection(scene, selection).changed);
  scene_grouping::TranslateTargets(scene, {{MvrNodeType::GroupObject,
      scene.fixtures.at(pasted.uuidRemap.at(fixtureId)).parentGroupUuid}}, {10, 20, 30});
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "copied-grouped.mvr").string()));
  CheckExport(dir / "copied-grouped.mvr", 11);

  MvrScene dangling = incoming;
  dangling.classes.erase(classId);
  std::vector<uint8_t> buffer{1, 2, 3};
  assert(!exporter.ExportCanonicalSnapshotToBuffer(dangling, buffer));
  assert(buffer.empty());
  assert(exporter.GetExportDiagnostics().front().code == MvrExportDiagnosticCode::StandardDataPreservationBlocked);
  config.GetScene() = dangling;
  assert(!config.SaveProject((dir / "roundtrip.pstg").string()));
  assert(config.LoadProject((dir / "roundtrip.pstg").string()));
  assert(config.GetScene().classes.size() == 3);
  std::string unsupported = xml;
  unsupported.insert(unsupported.find("name='  Shared"), "future='not represented' ");
  assert(tests::archive::WriteStoredZipWithRawNames(dir / "unsafe.mvr",
      {{"GeneralSceneDescription.xml", unsupported}, {"target.glb", tests::gdtf::BuildMinimalValidGlb()},
       {"fixture.gdtf", std::string(gdtf.begin(), gdtf.end())}}, error));
  MvrScene unsafe = Import(dir / "unsafe.mvr");
  assert(!unsafe.preservationIssues.empty());
  assert(!unsafe.preservationIssues.front().originalInformation.empty());
  assert(!exporter.ExportCanonicalSnapshotToBuffer(unsafe, buffer));
  config.GetScene() = unsafe;
  assert(!config.SaveProject((dir / "roundtrip.pstg").string()));
  assert(config.LoadProject((dir / "roundtrip.pstg").string()));
  MvrScene protectedMerge = incoming;
  mvr::MergeImportedSceneIntoCurrent(protectedMerge, unsafe);
  assert(!exporter.ExportCanonicalSnapshotToBuffer(protectedMerge, buffer));
  std::cout << "Class preservation, PSTG, merge and protection passed\n";
}
