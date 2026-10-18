/* This file is part of Perastage. */
#include "mvr_symdef_preservation.h"
#include "mvr_standard_preservation.h"
#include "mvr_model_dependencies.h"
#include "matrixutils.h"
#include "support/gdtf_test_fixture_builder.h"
#include <tinyxml2.h>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
constexpr auto defId = "a0000000-0000-4000-8000-000000000001";
constexpr auto nodeId = "b0000000-0000-4000-8000-000000000001";
constexpr auto symId = "c0000000-0000-4000-8000-000000000001";

static MvrScene Scan(const fs::path &directory, const std::string &definitions,
                     const std::string &objects = {}, const std::string &foreign = {}) {
  const std::string xml = "<GeneralSceneDescription>" + foreign + "<Scene><AUXData>" + definitions +
      "</AUXData><Layers><Layer><ChildList>" + objects + "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  MvrScene scene;
  scene.basePath = directory.string();
  mvr::RecordStandardPreservationRisks(*doc.RootElement(), scene);
  mvr::ReadMvrSymdefs(*doc.RootElement()->FirstChildElement("Scene"), scene,
                    [](const auto &file) { return file; });
  return scene;
}

static std::string Chunk(uint16_t type, const std::string &data) {
  std::string result;
  result += char(type & 255); result += char(type >> 8);
  const uint32_t length = static_cast<uint32_t>(data.size() + 6);
  for (int i = 0; i < 4; ++i) result += char((length >> (8 * i)) & 255);
  return result + data;
}

int main() {
  const auto dir = fs::temp_directory_path() / "perastage_symdef_preservation";
  fs::create_directories(dir);
  const auto bytes = tests::gdtf::BuildMinimalValidGlb();
  std::ofstream(dir / "geometry.glb", std::ios::binary).write(bytes.data(), bytes.size());
  const std::string geometry = "<Geometry3D fileName='geometry.glb'/>";
  const auto definition = "<Symdef uuid='" + std::string(defId) + "' name='  Exact  '><ChildList>" + geometry + "</ChildList></Symdef>";
  const auto symbol = "<Symbol uuid='" + std::string(symId) + "' symdef='" + defId + "'/>";
  const auto truss = "<Truss uuid='" + std::string(nodeId) + "'><Geometries>" + symbol + "</Geometries></Truss>";
  auto safe = Scan(dir, definition, truss);
  assert(safe.preservationIssues.empty());
  assert(safe.symdefs.at(defId).name == "  Exact  ");
  assert(mvr::CheckMvrSymdefs(safe).empty());
  for (const auto &scene : {
      Scan(dir, "<Symdef><ChildList/></Symdef>"),
      Scan(dir, "<Symdef uuid='00000000-0000-0000-0000-000000000000'><ChildList/></Symdef>"),
      Scan(dir, definition + definition),
      Scan(dir, definition, "<GroupObject uuid='" + std::string(defId) + "'/>"),
      Scan(dir, "<Symdef uuid='" + std::string(defId) + "'/>"),
      Scan(dir, "<Symdef uuid='" + std::string(defId) + "' future='data'><ChildList/></Symdef>"),
      Scan(dir, "<Symdef uuid='" + std::string(defId) + "'><ChildList future='data'/></Symdef>"),
      Scan(dir, "<Symdef uuid='" + std::string(defId) + "'><ChildList><Geometry3D fileName='absent.glb'/></ChildList></Symdef>"),
      Scan(dir, definition, truss + truss),
      Scan(dir, {}, truss),
      Scan(dir, definition, "<Truss uuid='" + std::string(nodeId) + "'><Geometries><Symbol symdef='" + defId + "'/></Geometries></Truss>"),
      Scan(dir, definition, "<Truss uuid='" + std::string(nodeId) + "'><Geometries>" + symbol + symbol + "</Geometries></Truss>")}) {
    assert(!scene.preservationIssues.empty());
    assert(!scene.preservationIssues.front().originalInformation.empty());
    assert(!scene.preservationIssues.front().recommendedAction.empty());
    assert(!mvr::CheckStandardPreservation(scene).empty());
  }
  assert(Scan(dir, definition, truss, "<UserData><Data provider='Foreign'>" + definition + truss +
      "</Data></UserData>").preservationIssues.empty());
  auto model = safe;
  Truss instance;
  instance.uuid = nodeId;
  instance.sourceRepresentation = Truss::GeometryRepresentation::SymbolSymdef;
  instance.sourceSymbolUuid = symId;
  instance.sourceSymdefUuid = defId;
  model.trusses.emplace(nodeId, instance);
  assert(mvr::CheckMvrSymdefs(model).empty());
  auto altered = model;
  altered.trusses.at(nodeId).sourceSymbolMatrix.o[0] = 1;
  assert(!mvr::CheckMvrSymdefs(altered).empty());
  altered = model;
  altered.classes.emplace(defId, MvrClass{});
  assert(!mvr::CheckMvrSymdefs(altered).empty());
  altered = model;
  altered.symdefs.erase(defId);
  assert(!mvr::CheckMvrSymdefs(altered).empty());
  tinyxml2::XMLDocument serialized;
  const auto output = "<Scene><AUXData>" + definition + "</AUXData><Layers><Layer><ChildList>" + truss + "</ChildList></Layer></Layers></Scene>";
  const auto reset = [&] { assert(serialized.Parse(output.c_str()) == tinyxml2::XML_SUCCESS); };
  const auto resource = [&](const std::string &file) { return dir / file; };
  const auto check = [&] { return mvr::CheckSerializedSymdefs(*serialized.RootElement(), model, {}, resource); };
  reset();
  assert(check().empty());
  auto *definitionNode = serialized.RootElement()->FirstChildElement("AUXData")->FirstChildElement();
  definitionNode->SetAttribute("name", "Changed");
  assert(!check().empty());
  reset();
  serialized.RootElement()->FirstChildElement("AUXData")->DeleteChildren();
  assert(!check().empty());
  reset();
  auto *geo = serialized.RootElement()->FirstChildElement("AUXData")->FirstChildElement()->FirstChildElement()->FirstChildElement();
  geo->SetAttribute("fileName", "absent.glb");
  assert(!check().empty());
  reset();
  auto *insert = serialized.RootElement()->FirstChildElement("Layers")->FirstChildElement()->FirstChildElement()->FirstChildElement()->FirstChildElement()->FirstChildElement();
  insert->SetAttribute("uuid", nodeId);
  assert(!check().empty());
  reset();
  insert = serialized.RootElement()->FirstChildElement("Layers")->FirstChildElement()->FirstChildElement()->FirstChildElement()->FirstChildElement()->FirstChildElement();
  auto *matrix = serialized.NewElement("Matrix");
  matrix->SetText("{1,0,0}{0,1,0}{0,0,1}{1,2,3}");
  insert->InsertEndChild(matrix);
  assert(!check().empty());
  assert(!mvr::SymdefMatrixPreservesSource("{1,0,0}{0,1,0}{0,0,1}{0.123456789,0,0}", Matrix{}));
  // Opacity maps also require preservation; the previous collector checked diffuse maps only.
  const auto material = Chunk(0xA000, std::string("Material\0", 9)) +
      Chunk(0xA210, Chunk(0xA300, std::string("mask.png\0", 9)));
  const auto threeDs = Chunk(0x4D4D, Chunk(0x3D3D, Chunk(0xAFFF, material)));
  std::ofstream(dir / "texture.3ds", std::ios::binary).write(threeDs.data(), threeDs.size());
  const auto dependencies = mvr::InspectModelDependencies(dir / "texture.3ds");
  assert(dependencies.error.empty());
  assert(dependencies.references == std::vector<std::string>{"mask.png"});
  assert(!mvr::SymdefResourceProblem(model, "texture.3ds").empty());
}
