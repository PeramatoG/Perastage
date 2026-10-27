/* This file is part of Perastage. */
#include "configmanager.h"
#include "inspection/xml_schema_validation.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvrscenemerger.h"
#include "scene_grouping.h"
#include "matrixutils.h"
#include "mvr_matrix_precision.h"
#include "layer_identity.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/mvr_symdef_fixture_builder.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include <wx/init.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace {
constexpr auto layerId = "11000000-0000-4000-8000-000000000001";
constexpr auto otherLayer = "11000000-0000-4000-8000-000000000002";
constexpr auto outer = "21000000-0000-4000-8000-000000000001";
constexpr auto inner = "21000000-0000-4000-8000-000000000002";
constexpr auto direct = "31000000-0000-4000-8000-000000000001";
constexpr auto nested = "31000000-0000-4000-8000-000000000002";
constexpr auto other = "31000000-0000-4000-8000-000000000003";
constexpr auto fixtureId = "51000000-0000-4000-8000-000000000001";
constexpr auto focusId = "61000000-0000-4000-8000-000000000001";
constexpr auto objectId = "61000000-0000-4000-8000-000000000002";
constexpr auto supportId = "71000000-0000-4000-8000-000000000001";
constexpr auto definition = "a1000000-0000-4000-8000-000000000001";
constexpr auto classId = "c1000000-0000-4000-8000-000000000001";
constexpr auto layerMatrix = "{1,0,0}{0,1,0}{0,0,1}{0,0,3000.1234567890123}";
constexpr auto local = "{1,0,0}{0,1,0}{0,0,1}{10,20,30}";
std::string Truss(const char *id) {
  std::string symbol = id; symbol[0] = '4';
  return "<Truss uuid='" + std::string(id) + "'><Matrix>" + local +
      "</Matrix><Classing>" + classId + "</Classing><Geometries><Symbol uuid='" + symbol +
      "' symdef='" + definition + "'/></Geometries><FixtureID>7</FixtureID><FixtureIDNumeric>7</FixtureIDNumeric></Truss>";
}
std::string OtherNodes() {
  const auto placement = "<Matrix>" + std::string(local) + "</Matrix>";
  const std::string geometry = "<Geometries><Geometry3D fileName='target.glb'/></Geometries>";
  return "<Fixture uuid='" + std::string(fixtureId) + "'>" + placement +
      "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode><Focus>" + focusId +
      "</Focus><FixtureID>4</FixtureID><FixtureIDNumeric>4</FixtureIDNumeric><UnitNumber>4</UnitNumber>"
      "<Addresses><Address>1</Address></Addresses></Fixture><FocusPoint uuid='" + focusId + "'>" +
      placement + geometry + "</FocusPoint><SceneObject uuid='" + objectId + "'>" + placement + geometry +
      "<FixtureID>5</FixtureID><FixtureIDNumeric>5</FixtureIDNumeric></SceneObject><Support uuid='" + supportId +
      "'>" + placement + geometry + "<ChainLength>100</ChainLength><FixtureID>6</FixtureID><FixtureIDNumeric>6</FixtureIDNumeric></Support>";
}
std::string Source() {
  return "<GeneralSceneDescription verMajor='1' verMinor='6' provider='LayerTest' providerVersion='1'>"
      "<Scene><AUXData><Class uuid='" + std::string(classId) + "' name='Rig'/><Symdef uuid='" + definition + "'><ChildList>"
      "<Geometry3D fileName='target.glb'/></ChildList></Symdef></AUXData><Layers><Layer uuid='" + std::string(layerId) + "' name='Elevated'><Matrix>" + layerMatrix +
      "</Matrix><ChildList>" + Truss(direct) + "<GroupObject uuid='" + outer + "'><Matrix>" +
      "{0,1,0}{-1,0,0}{0,0,1}{10,20,30}" +
      "</Matrix><ChildList><GroupObject uuid='" + inner + "'><Matrix>" +
      "{2,0,0}{0,1,0}{0,0,1}{10,20,30}" +
      "</Matrix><ChildList>" + Truss(nested) + "</ChildList></GroupObject></ChildList></GroupObject>"
      "</ChildList></Layer><Layer uuid='" + otherLayer + "' name='Other'><Matrix>"
      "{1,0,0}{0,1,0}{0,0,1}{0,0,700}</Matrix><ChildList>" + Truss(other) + OtherNodes() +
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
}
void Archive(const fs::path &path, const std::string &xml) {
  const auto fixture = path.parent_path() / "fixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithDmxMode("Mode", "Base").WriteArchive(fixture);
  std::ifstream file(fixture, std::ios::binary);
  const std::string gdtf(std::istreambuf_iterator<char>(file), {});
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, {{"GeneralSceneDescription.xml", xml}, {"target.glb", tests::mvr::TriangleGlb()}, {"fixture.gdtf", gdtf}}, error));
}
MvrScene Import(const fs::path &path) {
  MvrImporter importer;
  MvrImportResult result;
  MvrImportOptions options;
  options.promptConflicts = false; options.applyDictionary = false;
  assert(importer.ImportFromFile(path.string(), result, MvrImportMode::ParseOnly, options));
  return std::move(result.scene);
}
void Schema(const std::string &xml, bool exported = false) {
  using namespace perastage::inspection;
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  if (exported) doc.RootElement()->DeleteChild(doc.RootElement()->FirstChildElement("UserData"));
  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  const auto result = ValidateXmlAgainstSchema(printer.CStr(), Mvr16Schema());
  assert(result.xml.status == ValidationStatus::Valid);
  for (const auto &diagnostic : result.schema.diagnostics) std::cerr << diagnostic.message << '\n';
  assert(result.schema.status == ValidationStatus::Valid);
}
void Export(const MvrScene &scene, const fs::path &path) {
  MvrExporter exporter;
  const bool success = exporter.ExportCanonicalSnapshotToFile(scene, path.string());
  if (!success) {
    std::cerr << "Export failed: " << path << '\n';
    for (const auto &diagnostic : exporter.GetExportDiagnostics()) std::cerr << diagnostic.technicalDetail << '\n';
  }
  assert(success);
  std::string xml;
  assert(tests::zip::ReadEntry(path, "GeneralSceneDescription.xml", &xml));
  Schema(xml, true);
}
}
int main(int argc, char **argv) {
  wxInitializer initialize;
  assert(initialize.IsOk());
  const auto directory = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "perastage-layer-test";
  fs::create_directories(directory);
  Schema(Source());
  Archive(directory / "source.mvr", Source());
  auto scene = Import(directory / "source.mvr");
  Matrix authored, child;
  assert(MatrixUtils::ParseMatrix(layerMatrix, authored));
  assert(MatrixUtils::ParseMatrix(local, child));
  const auto expected = MatrixUtils::Multiply(authored, child);
  assert(scene.trusses.at(direct).transform.o == expected.o);
  assert(scene.preservationIssues.empty());
  assert(scene.fixtures.at(fixtureId).transform.o[2] == 730);
  assert(scene.supports.at(supportId).transform.o[2] == 730);
  assert(scene.sceneObjects.at(objectId).transform.o[2] == 730);
  assert(scene.sceneObjects.at(focusId).transform.o[2] == 730);
  assert(scene.layers.at(layerId).matrixSource->values[11] == 3000.1234567890123);
  assert(scene.trusses.at(nested).parentGroupUuid == inner);
  assert(scene.groupObjects.at(outer).layerUuid == layerId);
  assert(scene.trusses.at(direct).localTransform.o == child.o);
  Export(scene, directory / "unchanged.mvr");
  auto worldEdited = scene;
  worldEdited.trusses.at(direct).transform.o[2] += 64;
  worldEdited.fixtures.at(fixtureId).transform.o[2] += 64;
  worldEdited.supports.at(supportId).transform.o[2] += 64;
  worldEdited.sceneObjects.at(objectId).transform.o[2] += 64;
  worldEdited.sceneObjects.at(focusId).transform.o[2] += 64;
  Export(worldEdited, directory / "world-edited.mvr");
  auto groupEdited = scene;
  scene_grouping::TranslateTargets(groupEdited, {{MvrNodeType::GroupObject, outer}}, {17, 23, 31});
  Export(groupEdited, directory / "group-edited.mvr");
  Export(Import(directory / "group-edited.mvr"), directory / "group-edited-cycle.mvr");
  auto &config = ConfigManager::Get();
  config.Reset(); config.GetScene() = scene;
  for (int cycle = 1; cycle <= 3; ++cycle) {
    const auto project = directory / ("cycle" + std::to_string(cycle) + ".pstg");
    Export(config.GetScene(), directory / ("cycle" + std::to_string(cycle) + "-standalone.mvr"));
    assert(config.SaveProject(project.string()));
    config.Reset();
    assert(config.LoadProject(project.string()));
    assert(config.GetScene().layers.at(layerId).matrixSource->values[11] == 3000.1234567890123);
    Export(config.GetScene(), directory / ("cycle" + std::to_string(cycle) + ".mvr"));
  }
  Export(config.GetScene(), directory / "reopened.mvr");
  config.PushUndoState("translate elevated child");
  scene_grouping::TranslateTargets(config.GetScene(), {{MvrNodeType::Truss, direct}}, {17, 23, 31});
  Export(config.GetScene(), directory / "edited.mvr");
  assert(config.SaveProject((directory / "edited.pstg").string()));
  assert(config.CanUndo()); config.Undo();
  Export(config.GetScene(), directory / "undo.mvr");
  assert(config.CanRedo()); config.Redo();
  Export(config.GetScene(), directory / "redo.mvr");
  config.Reset();
  assert(config.LoadProject((directory / "edited.pstg").string()));
  Export(config.GetScene(), directory / "edited-reopened.mvr");

  auto regrouped = scene;
  scene_grouping::ObjectSelection selection;
  selection.trusses = {direct};
  assert(scene_grouping::AddSelectionToGroup(regrouped, selection, inner).changed);
  Export(regrouped, directory / "regrouped.mvr");
  assert(scene_grouping::RemoveSelectionFromGroup(regrouped, selection).changed);
  Export(regrouped, directory / "detached.mvr");
  selection.trusses = {nested};
  assert(scene_grouping::UngroupSelection(regrouped, selection).changed);
  Export(regrouped, directory / "ungrouped.mvr");
  config.Reset(); config.GetScene() = scene;
  config.PushUndoState("change hierarchy on elevated layer");
  selection.trusses = {direct};
  assert(scene_grouping::AddSelectionToGroup(config.GetScene(), selection, inner).changed);
  config.Undo(); Export(config.GetScene(), directory / "group-undo.mvr");
  config.Redo(); Export(config.GetScene(), directory / "group-redo.mvr");
  auto created = scene;
  selection.trusses = {nested, other};
  assert(scene_grouping::GroupSelection(created, selection).changed);
  Export(created, directory / "group-created.mvr");
  assert(scene_grouping::UngroupSelection(created, selection).changed);
  Export(created, directory / "promoted.mvr");
  auto layerMoved = scene;
  layerdomain::AssignLayer(layerMoved, layerMoved.trusses.at(direct), otherLayer);
  assert(MatrixUtils::Multiply(layerMoved.layers.at(otherLayer).transform,
      layerMoved.trusses.at(direct).localTransform).o == scene.trusses.at(direct).transform.o);
  Export(layerMoved, directory / "layer-moved.mvr");
  auto preparedLayer = scene;
  auto &preparedTruss = preparedLayer.trusses.at(direct);
  layerdomain::PrepareLayerEdit(preparedLayer, preparedTruss, otherLayer);
  layerdomain::CommitLayerEdit(preparedLayer, preparedTruss);
  assert(preparedTruss.localTransform.o == layerMoved.trusses.at(direct).localTransform.o);
  Export(preparedLayer, directory / "layer-prepared.mvr");

  const auto equivalent = mvr::AnalyzeImportedSceneMerge(scene, scene);
  assert(equivalent.skippedIncomingLayerUuids.contains(layerId));
  // A precision-only difference is still a distinct authored layer obligation.
  auto incoming = scene;
  incoming.layers.at(layerId).matrixSource->values[11] += 0.0000001;
  const auto analysis = mvr::AnalyzeImportedSceneMerge(scene, incoming);
  assert(analysis.layerUuidMap.at(layerId) != layerId);
  auto merged = scene;
  mvr::ApplyImportedSceneMergeAtomically(merged, incoming, analysis);
  assert(merged.trusses.size() == 6);
  assert(merged.fixtures.at(analysis.uuidMap.at(fixtureId)).focus == analysis.uuidMap.at(focusId));
  assert(merged.trusses.at(analysis.uuidMap.at(direct)).layerUuid == analysis.layerUuidMap.at(layerId));
  Export(merged, directory / "merged.mvr");

  MvrExporter exporter;
  for (const auto *bad : {
      "{0,1,0}{-1,0,0}{0,0,1}{0,0,1000}",
      "{1.0000000000000002,0,0}{0,1,0}{0,0,1}{0,0,1000}",
      "{1,0,0}{0,1,0}{0,0,1}{1,0,1000}",
      "{1,0,0}{0,1,0}{0,0,1}{0,0,nan}",
      "{1,0,0}{0,1,0}{0,0,1}{0,0,1e999}",
      "{1,0,0}{0,1,0}{0,0,1}{0,0,1000}</Matrix><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,2000}",
      "{1,0,0}{0,1,0}{0,0,1}{0,0,1000}<Extra/>"}) {
    auto xml = Source();
    xml.replace(xml.find(layerMatrix), std::string(layerMatrix).size(), bad);
    Archive(directory / "invalid.mvr", xml);
    const auto invalid = Import(directory / "invalid.mvr");
    assert(!invalid.preservationIssues.empty());
    std::vector<uint8_t> bytes{1};
    assert(!exporter.ExportCanonicalSnapshotToBuffer(invalid, bytes) && bytes.empty());
    assert(!exporter.GetExportDiagnostics().empty());
    config.GetScene() = scene;
    const auto project = directory / "retained.pstg";
    assert(config.SaveProject(project.string()));
    const auto read = [&] {
      std::ifstream file(project, std::ios::binary);
      return std::string(std::istreambuf_iterator<char>(file), {});
    };
    const auto before = read();
    config.GetScene() = invalid;
    assert(!config.SaveProject(project.string()));
    assert(config.GetLastProjectSaveFailure().stage == "SerializeScene");
    assert(read() == before);
  }
  // Unrelated unsupported standard fields and inconsistent ownership stay fatal.
  for (bool ownership : {false, true}) {
    auto invalid = scene;
    if (ownership) invalid.groupObjects.at(outer).children.push_back({MvrNodeType::Truss, nested});
    else invalid.preservationIssues.push_back({"Protocols", direct, "Truss/Protocols", "<Protocol/>",
        "Unsupported protocol assignment", "Keep the original MVR."});
    std::vector<uint8_t> bytes{1};
    assert(!exporter.ExportCanonicalSnapshotToBuffer(invalid, bytes) && bytes.empty());
  }
  config.Reset();
}
