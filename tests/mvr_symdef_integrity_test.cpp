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
#include "support/mvr_symdef_fixture_builder.h"
#include <tinyxml2.h>
#include <wx/init.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

namespace fs = std::filesystem;
using namespace perastage::inspection;
constexpr auto definitionId = "a0000000-0000-4000-8000-000000000001";
constexpr auto unusedId = "a0000000-0000-4000-8000-000000000002";
constexpr auto emptyId = "a0000000-0000-4000-8000-000000000003";
constexpr auto groupId = "20000000-0000-4000-8000-000000000001";
constexpr auto trussId = "30000000-0000-4000-8000-000000000001";
constexpr auto symbolId = "40000000-0000-4000-8000-000000000001";

static std::string Source(bool named = false) {
  std::string trusses;
  for (int id = 1; id <= 2; ++id)
    trusses += "<Truss uuid='30000000-0000-4000-8000-00000000000" + std::to_string(id) +
        "' name='Instance'><Matrix>{0,1,0}{-1,0,0}{0,0,1}{10,20,30}</Matrix><Geometries>"
        "<Symbol uuid='40000000-0000-4000-8000-00000000000" + std::to_string(id) +
        "' symdef='" + definitionId + "'/></Geometries><FixtureID>" + std::to_string(id) +
        "</FixtureID><FixtureIDNumeric>" + std::to_string(id) + "</FixtureIDNumeric></Truss>";
  return "<?xml version='1.0' encoding='UTF-8'?>"
      "<GeneralSceneDescription verMajor='1' verMinor='6' provider='IntegrityFixture' providerVersion='1'>"
      "<Scene><AUXData><Symdef uuid='" + std::string(definitionId) + "'" +
      (named ? " name='  Shared &amp; exact  '" : "") + "><ChildList>"
      "<Geometry3D fileName='shared.glb'/><Geometry3D fileName='second.glb'>"
      "<Matrix>{0,1,0}{-1,0,0}{0,0,1}{10,20,30}</Matrix></Geometry3D></ChildList></Symdef>"
      "<Symdef uuid='" + unusedId + "'><ChildList><Geometry3D fileName='unused.glb'>"
      "<Matrix>{1,0,0}{0,1,0}{0,0,1}{40,50,60}</Matrix></Geometry3D></ChildList></Symdef>"
      "<Symdef uuid='" + emptyId + "' name=''><ChildList/></Symdef></AUXData>"
      "<Layers><Layer uuid='10000000-0000-4000-8000-000000000001' name='Rig'><ChildList>"
      "<GroupObject uuid='" + groupId + "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{100,200,300}</Matrix>"
      "<ChildList>" + trusses + "</ChildList></GroupObject></ChildList></Layer></Layers>"
      "</Scene></GeneralSceneDescription>";
}

static void WriteSource(const fs::path &path, const std::string &xml,
                        const std::string &payload = tests::mvr::TriangleGlb()) {
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path,
      {{"GeneralSceneDescription.xml", xml}, {"shared.glb", payload},
       {"second.glb", tests::mvr::TriangleGlb()},
       {"unused.glb", tests::mvr::TriangleGlb()}}, error));
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

static void CheckSchema(const std::string &xml, bool exported) {
  const auto full = ValidateXmlAgainstSchema(xml, Mvr16Schema());
  assert(full.xml.status == ValidationStatus::Valid);
  if (!exported) {
    assert(full.schema.status == ValidationStatus::Valid);
    return;
  }
  assert(full.schema.status == ValidationStatus::Invalid);
  // The unchanged official XSD disallows published provider Data payloads.
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

static void CheckExport(const fs::path &path, int expectedDefinitions = 3,
                        int expectedSymbols = 2, bool named = false) {
  std::string xml;
  assert(tests::zip::ReadEntry(path, "GeneralSceneDescription.xml", &xml));
  CheckSchema(xml, true);
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  auto *scene = doc.RootElement()->FirstChildElement("Scene");
  auto *aux = scene->FirstChildElement("AUXData");
  assert(aux);
  std::map<std::string, tinyxml2::XMLElement *> definitions;
  for (auto *definition = aux->FirstChildElement("Symdef"); definition;
       definition = definition->NextSiblingElement("Symdef")) {
    definitions.emplace(definition->Attribute("uuid"), definition);
    assert(definition->FirstChildElement("ChildList"));
    for (auto *geo = definition->FirstChildElement("ChildList")->FirstChildElement("Geometry3D");
         geo; geo = geo->NextSiblingElement("Geometry3D")) {
      std::string bytes;
      assert(tests::zip::ReadEntry(path, geo->Attribute("fileName"), &bytes));
      assert(!bytes.empty());
    }
  }
  assert(static_cast<int>(definitions.size()) == expectedDefinitions);
  if (definitions.contains(definitionId)) {
    const auto *name = definitions.at(definitionId)->Attribute("name");
    if (named) assert(name && std::string(name) == "  Shared & exact  ");
    else assert(!name);
    assert(!definitions.at(unusedId)->Attribute("name"));
    assert(std::string(definitions.at(emptyId)->Attribute("name")).empty());
  }
  int count = 0;
  const auto visit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
    if (std::string(node->Name()) == "Symbol") {
      ++count;
      assert(definitions.contains(node->Attribute("symdef")));
    }
    for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement())
      self(self, child);
  };
  visit(visit, scene);
  assert(count == expectedSymbols);
}

int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "perastage_mvr_symdef_integrity";
  fs::create_directories(dir);
  CheckSchema(Source(), false);
  WriteSource(dir / "source.mvr", Source());
  MvrScene scene = Import(dir / "source.mvr");
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "unchanged.mvr").string()));
  CheckExport(dir / "unchanged.mvr");
  assert(scene.symdefs.size() == 3);
  assert(scene.preservationIssues.empty());
  CheckSchema(Source(true), false);
  WriteSource(dir / "named-source.mvr", Source(true));
  scene = Import(dir / "named-source.mvr");
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "named.mvr").string()));
  CheckExport(dir / "named.mvr", 3, 2, true);
  scene.trusses.at(trussId).name = "Unrelated edit";
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "edited.mvr").string()));
  CheckExport(dir / "edited.mvr", 3, 2, true);
  auto &config = ConfigManager::Get();
  config.Reset();
  config.GetScene() = scene;
  config.SetHiddenLayers({"10000000-0000-4000-8000-000000000001"});
  assert(exporter.ExportToFile((dir / "hidden.mvr").string()));
  CheckExport(dir / "hidden.mvr", 3, 2, true);
  assert(config.SaveProject((dir / "roundtrip.pstg").string()));
  assert(config.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::None);
  const auto extraction = scene.basePath;
  scene.Clear();
  assert(scene.symdefs.empty());
  config.Reset();
  assert(!fs::exists(extraction));
  assert(fs::remove(dir / "source.mvr"));
  assert(fs::remove(dir / "named-source.mvr"));
  assert(config.LoadProject((dir / "roundtrip.pstg").string()));
  assert(config.SaveProject((dir / "second-cycle.pstg").string()));
  assert(config.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::None);
  config.Reset();
  assert(config.LoadProject((dir / "second-cycle.pstg").string()));
  scene = config.GetScene();
  assert(scene.symdefs.size() == 3);
  // Recreate evidence only after reopening without either original archive.
  WriteSource(dir / "source.mvr", Source());
  WriteSource(dir / "named-source.mvr", Source(true));
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "reopened.mvr").string()));
  CheckExport(dir / "reopened.mvr", 3, 2, true);
  MvrScene incoming = Import(dir / "reopened.mvr");
  assert(exporter.ExportCanonicalSnapshotToFile(incoming, (dir / "repeated.mvr").string()));
  CheckExport(dir / "repeated.mvr", 3, 2, true);

  MvrScene shared = scene;
  const auto sharedAnalysis = mvr::AnalyzeImportedSceneMerge(shared, incoming);
  assert(sharedAnalysis.uuidMap.at(definitionId) == definitionId);
  assert(sharedAnalysis.uuidMap.at(symbolId) != symbolId);
  mvr::ApplyImportedSceneMergeAtomically(shared, incoming, sharedAnalysis);
  assert(shared.symdefs.size() == 3);
  assert(shared.trusses.at(trussId).sourceSymbolUuid == symbolId);
  const auto sharedTruss = sharedAnalysis.uuidMap.at(trussId);
  assert(shared.trusses.at(sharedTruss).sourceSymbolUuid == sharedAnalysis.uuidMap.at(symbolId));
  assert(exporter.ExportCanonicalSnapshotToFile(shared, (dir / "shared.mvr").string()));
  CheckExport(dir / "shared.mvr", 3, 4, true);

  MvrScene equivalent = incoming;
  equivalent.symdefs.at(emptyId).name.reset();
  MvrScene equalDefault = scene;
  const auto defaults = mvr::AnalyzeImportedSceneMerge(equalDefault, equivalent);
  assert(defaults.uuidMap.at(emptyId) == emptyId);
  mvr::ApplyImportedSceneMergeAtomically(equalDefault, equivalent, defaults);
  assert(equalDefault.symdefs.at(emptyId).name == "");

  // Same path/definition UUID and different valid bytes must not alias.
  std::string differentGlb = tests::mvr::TriangleGlb();
  differentGlb[differentGlb.find("generator\":\"A") + 12] = 'B';
  WriteSource(dir / "conflict-source.mvr", Source(true), differentGlb);
  MvrScene different = Import(dir / "conflict-source.mvr");
  MvrScene conflict = scene;
  const auto collision = mvr::AnalyzeImportedSceneMerge(conflict, different);
  const auto replacement = collision.uuidMap.at(definitionId);
  assert(replacement != definitionId);
  mvr::ApplyImportedSceneMergeAtomically(conflict, different, collision);
  assert(conflict.symdefs.size() == 4);
  assert(conflict.trusses.at(collision.uuidMap.at(trussId)).sourceSymdefUuid == replacement);
  assert(conflict.trusses.at(trussId).sourceSymdefUuid == definitionId);
  assert(exporter.ExportCanonicalSnapshotToFile(conflict, (dir / "conflict.mvr").string()));
  CheckExport(dir / "conflict.mvr", 4, 4, true);

  // A definition cannot replace an object with the same UUID under any policy.
  for (const auto policy : {mvr::MvrMergeUuidCollisionBehavior::GenerateStableUuid,
                           mvr::MvrMergeUuidCollisionBehavior::ReplaceExisting,
                           mvr::MvrMergeUuidCollisionBehavior::SkipIncoming}) {
    MvrScene target;
    GroupObject group;
    group.uuid = definitionId;
    target.groupObjects.emplace(group.uuid, group);
    mvr::MvrMergeOptions options;
    options.uuidCollisionBehavior = policy;
    const auto remap = mvr::AnalyzeImportedSceneMerge(target, incoming, options);
    assert(remap.uuidMap.at(definitionId) != definitionId);
    mvr::ApplyImportedSceneMergeAtomically(target, incoming, remap);
    assert(target.groupObjects.contains(definitionId));
    assert(target.symdefs.contains(remap.uuidMap.at(definitionId)));
    assert(target.trusses.at(trussId).sourceSymdefUuid == remap.uuidMap.at(definitionId));
    assert(exporter.ExportCanonicalSnapshotToFile(target, (dir / "cross-kind.mvr").string()));
  }
  SelectionState selected;
  selected.SetSelectedTrusses({trussId});
  scene_clipboard::Service clipboard;
  assert(clipboard.Capture(scene, selected, 1));
  const auto pasted = clipboard.Paste(scene, 1);
  assert(pasted.changed);
  const auto copiedUuid = pasted.uuidRemap.at(trussId);
  assert(scene.trusses.at(copiedUuid).sourceSymbolUuid != symbolId);
  assert(scene.trusses.at(copiedUuid).sourceSymdefUuid == definitionId);
  assert(scene.symdefs.size() == 3);
  scene_grouping::ObjectSelection selection;
  selection.trusses = {copiedUuid, trussId};
  assert(scene_grouping::GroupSelection(scene, selection).changed);
  scene_grouping::TranslateTargets(scene, {{MvrNodeType::GroupObject, groupId}}, {5, 6, 7});
  assert(exporter.ExportCanonicalSnapshotToFile(scene, (dir / "copied-grouped.mvr").string()));
  CheckExport(dir / "copied-grouped.mvr", 3, 3, true);

  const auto projectPath = dir / "roundtrip.pstg";
  const auto readProject = [&] {
    std::ifstream input(projectPath, std::ios::binary);
    assert(input.is_open());
    return std::string(std::istreambuf_iterator<char>(input), {});
  };
  const auto originalProject = readProject();
  const auto checkRejectedSave = [&](const MvrScene &rejectedScene,
                                     ProjectSaveFailureKind kind,
                                     const std::string &entry = {}) {
    config.GetScene() = rejectedScene;
    assert(!config.SaveProject(projectPath.string()));
    assert(config.GetLastProjectSaveFailure().kind == kind);
    assert(config.GetLastProjectSaveFailure().stage == "SerializeScene");
    assert(!config.GetLastProjectSaveFailure().detail.empty());
    if (!entry.empty()) assert(config.GetLastProjectSaveFailure().entry == entry);
    assert(readProject() == originalProject);
    assert(!fs::exists(projectPath.string() + ".tmp"));
    assert(config.LoadProject(projectPath.string()));
    assert(config.GetScene().symdefs.size() == 3);
  };
  MvrScene missing = incoming;
  missing.symdefs.at(unusedId).geometries.front().file = "missing.glb";
  std::vector<uint8_t> bytes{1, 2, 3};
  assert(!exporter.ExportCanonicalSnapshotToBuffer(missing, bytes));
  assert(bytes.empty());
  // Losing an incoming resource must never bind its definition to a target namesake.
  const auto lostPath = fs::path(different.basePath) /
      different.symdefs.at(definitionId).geometries.front().file;
  assert(fs::remove(lostPath));
  MvrScene missingMerge = incoming;
  mvr::MergeImportedSceneIntoCurrent(missingMerge, different);
  assert(!exporter.ExportCanonicalSnapshotToBuffer(missingMerge, bytes));
  assert(bytes.empty());
  assert(!missingMerge.preservationIssues.empty());
  bool resourceFailure = false;
  for (const auto &diagnostic : exporter.GetExportDiagnostics()) {
    if (diagnostic.code != MvrExportDiagnosticCode::ResourceMissing) continue;
    assert(diagnostic.resourceName == different.symdefs.at(definitionId).geometries.front().file);
    resourceFailure = true;
  }
  assert(resourceFailure);
  checkRejectedSave(missingMerge, ProjectSaveFailureKind::Resource,
                    different.symdefs.at(definitionId).geometries.front().file);
  checkRejectedSave(missing, ProjectSaveFailureKind::Resource);
  MvrScene dangling = incoming;
  dangling.symdefs.erase(definitionId);
  assert(!exporter.ExportCanonicalSnapshotToBuffer(dangling, bytes));
  std::string unsafeXml = Source(true);
  unsafeXml.replace(unsafeXml.find("unused.glb"), 10, "absent.glb");
  WriteSource(dir / "unsafe.mvr", unsafeXml);
  MvrScene unsafe = Import(dir / "unsafe.mvr");
  assert(!unsafe.preservationIssues.empty());
  assert(!unsafe.preservationIssues.front().originalInformation.empty());
  MvrScene protectedMerge = incoming;
  mvr::MergeImportedSceneIntoCurrent(protectedMerge, unsafe);
  assert(!exporter.ExportCanonicalSnapshotToBuffer(protectedMerge, bytes));
  std::vector<std::string> protectedStructures;
  auto nested = Source(true);
  nested.insert(nested.find("</AUXData>"),
      "<Symdef uuid='a0000000-0000-4000-8000-000000000004' name='Nested'><ChildList>"
      "<Symbol uuid='40000000-0000-4000-8000-000000000004' symdef='" + std::string(definitionId) +
      "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{9,8,7}</Matrix></Symbol></ChildList></Symdef>");
  protectedStructures.push_back(nested);
  auto mixed = Source(true);
  mixed.insert(mixed.find("<Geometries>") + 12, "<Geometry3D fileName='unused.glb'/>");
  protectedStructures.push_back(mixed);
  auto multiple = Source(true);
  multiple.insert(multiple.find("</Geometries>"), "<Symbol uuid='40000000-0000-4000-8000-000000000004' symdef='" +
      std::string(definitionId) + "'/>");
  protectedStructures.push_back(multiple);
  auto placement = Source(true);
  const auto ref = "symdef='" + std::string(definitionId) + "'/>";
  placement.replace(placement.find(ref), ref.size(), "symdef='" + std::string(definitionId) +
      "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{1,2,3}</Matrix></Symbol>");
  protectedStructures.push_back(placement);
  auto geometryPlacement = Source(true);
  const std::string direct = "<Geometry3D fileName='shared.glb'/>";
  geometryPlacement.replace(geometryPlacement.find(direct), direct.size(),
      "<Geometry3D fileName='shared.glb'><Matrix>{1,0,0}{0,1,0}{0,0,1}{1,2,3}</Matrix></Geometry3D>");
  protectedStructures.push_back(geometryPlacement);
  auto precise = Source(true);
  precise.replace(precise.find("{40,50,60}"), 10, "{40.123456789,50,60}");
  protectedStructures.push_back(precise);
  for (const auto &kind : {"SceneObject", "FocusPoint", "Support"}) {
    auto projected = Source(true);
    const std::string open = std::string("<") + kind + " uuid='f0000000-0000-4000-8000-000000000001'>";
    std::string object = open + "<Geometries><Symbol uuid='f0000000-0000-4000-8000-000000000002' symdef='" +
        definitionId + "'/></Geometries>";
    if (std::string(kind) == "Support") object += "<ChainLength>1000</ChainLength>";
    if (std::string(kind) != "FocusPoint") object += "<FixtureID>3</FixtureID><FixtureIDNumeric>3</FixtureIDNumeric>";
    object += std::string("</") + kind + ">";
    projected.insert(projected.find("</ChildList></GroupObject>"), object);
    protectedStructures.push_back(projected);
  }
  for (const auto &protectedXml : protectedStructures) {
    CheckSchema(protectedXml, false);
    WriteSource(dir / "protected.mvr", protectedXml);
    auto protectedScene = Import(dir / "protected.mvr");
    assert(!protectedScene.preservationIssues.empty());
    assert(!exporter.ExportCanonicalSnapshotToBuffer(protectedScene, bytes));
    checkRejectedSave(protectedScene, ProjectSaveFailureKind::MvrPreservation);
  }
  assert(config.SaveProject(projectPath.string()));
  assert(config.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::None);
  std::cout << "Symdef definitions, resources, instances, merge and protection passed\n";
}
