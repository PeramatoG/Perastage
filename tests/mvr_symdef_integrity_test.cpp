#include "support/mvr_recovery_assertions.h"
/* This file is part of Perastage. */
#include "configmanager.h"
#include "inspection/xml_schema_validation.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvrscenemerger.h"
#include "mvr_matrix_precision.h"
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

void RunMatrixPrecisionRegression(const std::filesystem::path &directory);

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

// Check source identities, hierarchy, numeric placement and actual resource
// bytes independently of the display projection or exporter validation.
static void CheckRetainedStructure(const std::string &sourceXml,
                                   const fs::path &source,
                                   const fs::path &output) {
  std::string exported;
  assert(
      tests::zip::ReadEntry(output, "GeneralSceneDescription.xml", &exported));
  CheckSchema(exported, true);
  tinyxml2::XMLDocument before, after;
  assert(before.Parse(sourceXml.c_str()) == tinyxml2::XML_SUCCESS);
  assert(after.Parse(exported.c_str()) == tinyxml2::XML_SUCCESS);
  const auto index = [](tinyxml2::XMLElement *root) {
    std::map<std::string, tinyxml2::XMLElement *> result;
    const auto visit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
      if (std::string(node->Name()) == "UserData")
        return;
      if (const auto *id = node->Attribute("uuid"))
        result[id] = node;
      for (auto *child = node->FirstChildElement(); child;
           child = child->NextSiblingElement())
        self(self, child);
    };
    visit(visit, root);
    return result;
  };
  const auto original = index(before.RootElement()),
             actual = index(after.RootElement());
  const auto matrix = [](tinyxml2::XMLElement *node) {
    auto *value = node->FirstChildElement("Matrix");
    auto parsed = mvr::ParseCanonicalMatrix(
        value ? value->GetText() : "{1,0,0}{0,1,0}{0,0,1}{0,0,0}");
    assert(parsed);
    return parsed->values;
  };
  const auto owner = [](tinyxml2::XMLElement *node) {
    auto *container = node->Parent() ? node->Parent()->ToElement() : nullptr;
    auto *parent = container && container->Parent()
                       ? container->Parent()->ToElement()
                       : nullptr;
    return parent && parent->Attribute("uuid")
               ? std::string(parent->Attribute("uuid"))
               : std::string();
  };
  for (const auto &[id, node] : original) {
    assert(actual.contains(id));
    auto *copy = actual.at(id);
    assert(std::string(node->Name()) == copy->Name() &&
           owner(node) == owner(copy));
    assert(matrix(node) == matrix(copy));
    if (const auto *reference = node->Attribute("symdef"))
      assert(copy->Attribute("symdef") &&
             std::string(reference) == copy->Attribute("symdef"));
    const auto *containerName =
        std::string(node->Name()) == "Symdef" ? "ChildList" : "Geometries";
    auto *first = node->FirstChildElement(containerName),
         *second = copy->FirstChildElement(containerName);
    if (!first)
      continue;
    assert(second);
    auto *geometry = first->FirstChildElement("Geometry3D");
    auto *retained = second->FirstChildElement("Geometry3D");
    for (; geometry && retained;
         geometry = geometry->NextSiblingElement("Geometry3D"),
         retained = retained->NextSiblingElement("Geometry3D")) {
      assert(matrix(geometry) == matrix(retained));
      std::string sourceBytes, outputBytes;
      assert(tests::zip::ReadEntry(source, geometry->Attribute("fileName"),
                                   &sourceBytes));
      assert(tests::zip::ReadEntry(output, retained->Attribute("fileName"),
                                   &outputBytes));
      assert(sourceBytes == outputBytes);
    }
    assert(!geometry && !retained);
  }
}

int main(int argc, char **argv) {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "perastage_mvr_symdef_integrity";
  fs::create_directories(dir);
  RunMatrixPrecisionRegression(dir);
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
  const auto checkRecoveredSave = [&](const MvrScene &repairedScene) {
    config.GetScene() = repairedScene;
    const auto copy = dir / "repaired.pstg";
    assert(config.SaveProject(copy.string()));
    assert(config.LoadProject(copy.string()));
    assert(config.GetScene().fixtures.size() == repairedScene.fixtures.size());
    assert(!fs::exists(copy.string() + ".tmp"));
    assert(readProject() == originalProject);
  };
  MvrScene missing = incoming;
  missing.symdefs.at(unusedId).geometries.front().file = "missing.glb";
  std::vector<uint8_t> bytes{1, 2, 3};
  auto repaired =
      tests::recovery::Export(exporter, missing, dir / "missing-model.mvr");
  assert(tests::recovery::Count(repaired, "Symdef") == missing.symdefs.size());
  // Losing an incoming resource must never bind its definition to a target namesake.
  const auto lostPath = fs::path(different.basePath) /
      different.symdefs.at(definitionId).geometries.front().file;
  assert(fs::remove(lostPath));
  MvrScene missingMerge = incoming;
  mvr::MergeImportedSceneIntoCurrent(missingMerge, different);
  const auto recoveredMerge = tests::recovery::Export(
      exporter, missingMerge, dir / "missing-merged.mvr");
  tinyxml2::XMLDocument recoveredDocument;
  assert(recoveredDocument.Parse(recoveredMerge.c_str()) ==
         tinyxml2::XML_SUCCESS);
  auto *aux = recoveredDocument.RootElement()
                  ->FirstChildElement("Scene")
                  ->FirstChildElement("AUXData");
  assert(aux);
  size_t checkedAcquisitions = 0;
  for (const auto &issue : missingMerge.preservationIssues)
    if (issue.kind == MvrPreservationIssueKind::ResourceFailure &&
        missingMerge.symdefs.contains(issue.objectUuid)) {
      const auto &definition = missingMerge.symdefs.at(issue.objectUuid);
      const auto removed =
          std::count_if(definition.geometries.begin(),
                        definition.geometries.end(), [&](const auto &geometry) {
                          return geometry.file == issue.originalInformation;
                        });
      if (!removed)
        continue;
      auto *owner = aux->FirstChildElement("Symdef");
      while (owner && std::string(owner->Attribute("uuid")) != issue.objectUuid)
        owner = owner->NextSiblingElement("Symdef");
      assert(owner);
      size_t actual = 0;
      for (auto *geometry = owner->FirstChildElement("ChildList")
                                ->FirstChildElement("Geometry3D");
           geometry; geometry = geometry->NextSiblingElement("Geometry3D"))
        ++actual;
      assert(actual == definition.geometries.size() - removed);
      ++checkedAcquisitions;
    }
  assert(checkedAcquisitions > 0);
  assert(!missingMerge.preservationIssues.empty());
  checkRecoveredSave(missingMerge);
  checkRecoveredSave(missing);
  MvrScene dangling = incoming;
  dangling.symdefs.erase(definitionId);
  tests::recovery::Export(exporter, dangling, dir / "dangling-definition.mvr");
  std::string unsafeXml = Source(true);
  unsafeXml.replace(unsafeXml.find("unused.glb"), 10, "absent.glb");
  WriteSource(dir / "unsafe.mvr", unsafeXml);
  MvrScene unsafe = Import(dir / "unsafe.mvr");
  assert(!unsafe.preservationIssues.empty());
  assert(!unsafe.preservationIssues.front().originalInformation.empty());
  MvrScene protectedMerge = incoming;
  mvr::MergeImportedSceneIntoCurrent(protectedMerge, unsafe);
  tests::recovery::Export(exporter, protectedMerge, dir / "repaired-merge.mvr");
  std::vector<std::string> retainedStructures;
  auto nested = Source(true);
  nested.insert(nested.find("</AUXData>"),
      "<Symdef uuid='a0000000-0000-4000-8000-000000000004' name='Nested'><ChildList>"
      "<Symbol uuid='40000000-0000-4000-8000-000000000004' symdef='" + std::string(definitionId) +
      "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{9,8,7}</Matrix></Symbol></ChildList></Symdef>");
  retainedStructures.push_back(nested);
  auto mixed = Source(true);
  mixed.insert(mixed.find("<Geometries>") + 12, "<Geometry3D fileName='unused.glb'/>");
  retainedStructures.push_back(mixed);
  auto multiple = Source(true);
  multiple.insert(multiple.find("</Geometries>"), "<Symbol uuid='40000000-0000-4000-8000-000000000004' symdef='" +
      std::string(definitionId) + "'/>");
  retainedStructures.push_back(multiple);
  auto placement = Source(true);
  const auto ref = "symdef='" + std::string(definitionId) + "'/>";
  placement.replace(placement.find(ref), ref.size(), "symdef='" + std::string(definitionId) +
      "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{1,2,3}</Matrix></Symbol>");
  retainedStructures.push_back(placement);
  auto geometryPlacement = Source(true);
  const std::string direct = "<Geometry3D fileName='shared.glb'/>";
  geometryPlacement.replace(geometryPlacement.find(direct), direct.size(),
      "<Geometry3D fileName='shared.glb'><Matrix>{1,0,0}{0,1,0}{0,0,1}{1,2,3}</Matrix></Geometry3D>");
  retainedStructures.push_back(geometryPlacement);
  auto precise = Source(true);
  precise.replace(precise.find("{40,50,60}"), 10, "{40.123456789,50,60}");
  WriteSource(dir / "precise-definition.mvr", precise);
  auto preciseScene = Import(dir / "precise-definition.mvr");
  assert(preciseScene.preservationIssues.empty());
  assert(exporter.ExportCanonicalSnapshotToBuffer(preciseScene, bytes));
  for (const auto &kind : {"SceneObject", "FocusPoint", "Support"}) {
    auto projected = Source(true);
    const std::string open = std::string("<") + kind + " uuid='f0000000-0000-4000-8000-000000000001'>";
    std::string object = open + "<Geometries><Symbol uuid='f0000000-0000-4000-8000-000000000002' symdef='" +
        definitionId + "'/></Geometries>";
    if (std::string(kind) == "Support") object += "<ChainLength>1000</ChainLength>";
    if (std::string(kind) != "FocusPoint") object += "<FixtureID>3</FixtureID><FixtureIDNumeric>3</FixtureIDNumeric>";
    object += std::string("</") + kind + ">";
    projected.insert(projected.find("</ChildList></GroupObject>"), object);
    retainedStructures.push_back(projected);
  }
  size_t structureIndex = 0;
  for (const auto &retainedXml : retainedStructures) {
    CheckSchema(retainedXml, false);
    WriteSource(dir / "retained-source.mvr", retainedXml);
    auto retainedScene = Import(dir / "retained-source.mvr");
    const auto output =
        dir / ("retained-" + std::to_string(structureIndex++) + ".mvr");
    assert(
        exporter.ExportCanonicalSnapshotToFile(retainedScene, output.string()));
    CheckRetainedStructure(retainedXml, dir / "retained-source.mvr", output);
    config.GetScene() = retainedScene;
    const auto copy = dir / "retained.pstg";
    assert(config.SaveProject(copy.string()) &&
           config.LoadProject(copy.string()));
    assert(exporter.ExportCanonicalSnapshotToFile(config.GetScene(),
                                                  output.string()));
    CheckRetainedStructure(retainedXml, dir / "retained-source.mvr", output);
  }
  assert(config.LoadProject(projectPath.string()));
  assert(config.SaveProject(projectPath.string()));
  assert(config.GetLastProjectSaveFailure().kind == ProjectSaveFailureKind::None);
  std::cout << "Symdef definitions, resources, instances, merge and protection passed\n";
}
