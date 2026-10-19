/* This file is part of Perastage. */
#include "configmanager.h"
#include "inspection/xml_schema_validation.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvrscenemerger.h"
#include "mvr_matrix_precision.h"
#include "scene_grouping.h"
#include "matrixutils.h"
#include "support/archive_entry_test_utils.h"
#include "support/mvr_symdef_fixture_builder.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include <cassert>
#include <cmath>
#include <limits>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace {
namespace fs = std::filesystem;
constexpr auto definition = "a1000000-0000-4000-8000-000000000001";
constexpr auto outer = "21000000-0000-4000-8000-000000000001";
constexpr auto inner = "21000000-0000-4000-8000-000000000002";
constexpr auto precise = "{0.81234567890123456,0.31234567890123456,0}{-0.31234567890123456,0.81234567890123456,0}{0,0,1.1234567890123457}{12345.678901234567,-23456.78901234567,34567.89012345679}";
constexpr auto identity = "{1,0,0}{0,1,0}{0,0,1}{0,0,0}";
std::string Id(char kind, int index) {
  std::ostringstream text;
  text << kind << "1000000-0000-4000-8000-" << std::setfill('0') << std::setw(12) << index;
  return text.str();
}
std::string Source() {
  std::string trusses;
  for (int i = 1; i <= 19; ++i) {
    trusses += "<Truss uuid='" + Id('3', i) + "' name='Precision instance'>";
    if (i % 3) trusses += "<Matrix>" + std::string(i % 3 == 1 ? precise : identity) + "</Matrix>";
    trusses += "<Geometries><Symbol uuid='" + Id('4', i) + "' symdef='" + definition + "'>";
    if (i % 2) trusses += "<Matrix>" + std::string(identity) + "</Matrix>";
    trusses += "</Symbol></Geometries><FixtureID>" + std::to_string(i) + "</FixtureID>"
        "<FixtureIDNumeric>" + std::to_string(i) + "</FixtureIDNumeric></Truss>";
  }
  return "<GeneralSceneDescription verMajor='1' verMinor='6' provider='PrecisionFixture' providerVersion='1'>"
      "<Scene><AUXData><Symdef uuid='" + std::string(definition) + "' name='Shared'><ChildList>"
      "<Geometry3D fileName='precision.glb'/><Geometry3D fileName='precision.glb'><Matrix>" + precise +
      "</Matrix></Geometry3D></ChildList></Symdef></AUXData><Layers>"
      "<Layer uuid='11000000-0000-4000-8000-000000000001' name='Precision'><ChildList>"
      "<GroupObject uuid='" + outer + "'><Matrix>" + precise + "</Matrix><ChildList>"
      "<GroupObject uuid='" + inner + "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{9.8765432109876543,8.7654321098765432,7.6543210987654321}</Matrix>"
      "<ChildList>" + trusses + "</ChildList></GroupObject></ChildList></GroupObject>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
}
void Archive(const fs::path &path, const std::string &xml) {
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, {{"GeneralSceneDescription.xml", xml},
      {"precision.glb", tests::mvr::TriangleGlb()}}, error));
}
MvrScene Import(const fs::path &path) {
  MvrImporter importer;
  MvrImportResult result;
  MvrImportOptions options;
  options.promptConflicts = false; options.applyDictionary = false;
  assert(importer.ImportFromFile(path.string(), result, MvrImportMode::ParseOnly, options));
  return std::move(result.scene);
}
void Schema(const std::string &xml, bool exported) {
  using namespace perastage::inspection;
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  if (exported) doc.RootElement()->DeleteChild(doc.RootElement()->FirstChildElement("UserData"));
  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  const auto result = ValidateXmlAgainstSchema(printer.CStr(), Mvr16Schema());
  assert(result.xml.status == ValidationStatus::Valid);
  assert(result.schema.status == ValidationStatus::Valid);
}
void Export(const MvrScene &scene, const fs::path &path) {
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, path.string()));
  std::string xml;
  assert(tests::zip::ReadEntry(path, "GeneralSceneDescription.xml", &xml));
  Schema(xml, true);
}
}

void RunMatrixPrecisionRegression(const std::filesystem::path &directory) {
  const auto dir = directory / "precision";
  fs::create_directories(dir);
  Schema(Source(), false);
  Archive(dir / "source.mvr", Source());
  auto scene = Import(dir / "source.mvr");
  assert(scene.trusses.size() == 19 && scene.groupObjects.size() == 2);
  assert(scene.preservationIssues.empty());
  for (const auto &[uuid, truss] : scene.trusses) {
    assert(truss.parentGroupUuid == inner);
    const auto expected = MatrixUtils::Multiply(scene.groupObjects.at(inner).transform, truss.localTransform);
    assert(expected.u == truss.transform.u && expected.o == truss.transform.o);
  }
  Export(scene, dir / "unchanged.mvr");
  for (int cycle = 1; cycle <= 3; ++cycle) {
    auto reopened = Import(dir / (cycle == 1 ? "unchanged.mvr" : "cycle" + std::to_string(cycle - 1) + ".mvr"));
    Export(reopened, dir / ("cycle" + std::to_string(cycle) + ".mvr"));
  }
  auto &config = ConfigManager::Get();
  config.Reset(); config.GetScene() = scene;
  MvrExporter exporter;
  assert(exporter.ExportToFile((dir / "standalone.mvr").string()));
  assert(config.SaveProject((dir / "roundtrip.pstg").string()));
  config.Reset();
  assert(config.LoadProject((dir / "roundtrip.pstg").string()));
  assert(config.SaveProject((dir / "second-cycle.pstg").string()));
  Export(config.GetScene(), dir / "reopened.mvr");
  auto edited = scene;
  auto &truss = edited.trusses.at(Id('3', 1));
  Matrix world = truss.transform;
  // A one-ULP edit must not be hidden by the transform repair tolerance.
  world.o[0] = std::nextafter(world.o[0], std::numeric_limits<float>::infinity());
  truss.transform = world;
  Export(edited, dir / "world-edited.mvr");
  scene_grouping::TranslateTargets(edited, {{MvrNodeType::Truss, Id('3', 1)}}, {17, 23, 31});
  Export(edited, dir / "edited.mvr");
  Export(Import(dir / "edited.mvr"), dir / "edited-cycle.mvr");
  config.GetScene() = edited;
  assert(config.SaveProject((dir / "edited.pstg").string()));
  config.Reset();
  assert(config.LoadProject((dir / "edited.pstg").string()));
  Export(config.GetScene(), dir / "edited-reopened.mvr");
  for (const bool rotate : {true, false}) {
    auto changed = scene;
    auto changedWorld = changed.trusses.at(Id('3', 1)).transform;
    const auto insertion = changedWorld.o;
    if (rotate) changedWorld = MatrixUtils::Multiply(MatrixUtils::EulerToMatrix(17, 23, 31), changedWorld);
    else for (auto *axis : {&changedWorld.u, &changedWorld.v, &changedWorld.w})
      for (float &value : *axis) value *= 1.125f;
    changedWorld.o = insertion;
    scene_grouping::SetTargetWorldTransform(changed, {MvrNodeType::Truss, Id('3', 1)}, changedWorld);
    Export(changed, dir / (rotate ? "rotated.mvr" : "scaled.mvr"));
  }
  auto parentEdited = scene;
  scene_grouping::TranslateTargets(parentEdited, {{MvrNodeType::GroupObject, outer}}, {17, 23, 31});
  Export(parentEdited, dir / "parent-edited.mvr");
  auto definitionEdited = scene;
  definitionEdited.symdefs.at(definition).geometries.at(1).transform.o[0] = 8765.125f;
  Export(definitionEdited, dir / "definition-edited.mvr");
  auto incoming = scene;
  incoming.symdefs.at(definition).geometries.at(1).matrixSource->values[9] += 0.0000001;
  const auto analysis = mvr::AnalyzeImportedSceneMerge(scene, incoming);
  assert(analysis.uuidMap.at(definition) != definition);
  mvr::ApplyImportedSceneMergeAtomically(scene, incoming, analysis);
  assert(scene.symdefs.size() == 2);
  Export(scene, dir / "merged.mvr");
  for (const auto &bad : {std::string("{1,0,0}{0,1,0}{0,0,1}{nan,0,0}"),
      std::string("{1,0,0}{0,1,0}{0,0,1}{1e999,0,0}"),
      std::string("{1 0 0}{0 1 0}{0 0 1}{0 0 0}"),
      std::string(precise) + "garbage"}) {
    auto xml = Source(); xml.replace(xml.find(precise), std::string(precise).size(), bad);
    Archive(dir / "invalid.mvr", xml);
    const auto invalid = Import(dir / "invalid.mvr");
    assert(!invalid.preservationIssues.empty());
    std::vector<uint8_t> bytes{1};
    assert(!exporter.ExportCanonicalSnapshotToBuffer(invalid, bytes) && bytes.empty());
    assert(!exporter.GetExportDiagnostics().empty());
    config.GetScene() = invalid;
    assert(!config.SaveProject((dir / "invalid.pstg").string()));
    assert(config.GetLastProjectSaveFailure().stage == "SerializeScene");
  }
  config.Reset();
}
