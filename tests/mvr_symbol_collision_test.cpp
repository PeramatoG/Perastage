/* This file is part of Perastage. */
#include "configmanager.h"
#include "mvrimporter.h"
#include "mvrexporter.h"
#include "mvr_source_structure_service.h"
#include "mvr_symdef_preservation.h"
#include "mvr_matrix_precision.h"
#include "loader3ds.h"
#include "support/archive_entry_test_utils.h"
#include "support/zip_test_utils.h"
#include "support/mvr_structure_fixture.h"
#include <tinyxml2.h>
#include <algorithm>
#include <bit>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace {
namespace fs = std::filesystem;
using Entries = std::vector<std::pair<std::string, std::string>>;
std::string Id(int kind, int number) {
  auto value = std::to_string(number);
  return std::to_string(kind) + "abc0000-0000-4000-8000-" +
         std::string(12 - value.size(), '0') + value;
}
std::string XmlMatrix(int x) { return tests::mvr_structure::Matrix(x, 2, 3); }
std::string Symbol(const std::string &id, const std::string &target, int x) {
  const auto upper = [](std::string value) {
    for (auto &c : value) if (c >= 'a' && c <= 'f') c = char(c - 'a' + 'A');
    return value;
  };
  return "<Symbol uuid='" + upper(id) + "' symdef='" + upper(target) + "'>" + XmlMatrix(x) + "</Symbol>";
}
// A real triangle, with distinct authored object names and float coordinates.
std::string Triangle(int index) {
  const auto word = [](uint16_t value) {
    return std::string{char(value & 255), char(value >> 8)};
  };
  std::string vertices = word(3);
  for (float value : {0.f, 0.f, 0.f, float(index + 1), 0.f, 0.f, 0.f, 1.f, 0.f}) {
    auto bits = std::bit_cast<uint32_t>(value);
    for (int byte = 0; byte < 4; ++byte) vertices += char(bits >> (8 * byte));
  }
  const auto faces = word(1) + word(0) + word(1) + word(2) + word(0);
  using tests::mvr_structure::Chunk;
  return Chunk(0x4d4d, Chunk(0x3d3d, Chunk(0x4000,
      "Triangle" + std::to_string(index) + std::string(1, '\0') +
      Chunk(0x4100, Chunk(0x4110, vertices) + Chunk(0x4120, faces)))));
}
Entries BuildCollisionFixture(bool invalid) {
  Entries entries;
  std::string definitions, nodes;
  for (int i = 0; i < 6; ++i) {
    const auto file = "model" + std::to_string(i) + ".3ds";
    entries.emplace_back(file, Triangle(i));
    definitions += "<Symdef uuid='" + Id(1, i) + "' name='Definition " +
        std::to_string(i) + "'><ChildList><Geometry3D fileName='" + file + "'>" +
        XmlMatrix(i + 1) + "</Geometry3D></ChildList></Symdef>";
  }
  for (int i = 0; i < 34; ++i) {
    const std::string kind = i < 32 ? "Truss" : "SceneObject";
    nodes += "<" + kind + " uuid='" + Id(2, i) + "' name='Instance'>" + XmlMatrix(i) +
        "<Geometries>" + Symbol(Id(1, (i == 33 ? i + 1 : i) % 6), Id(1, i % 6), 5) +
        "</Geometries><FixtureID>" + std::to_string(i) + "</FixtureID></" + kind + ">";
  }
  if (invalid) {
    // Independent valid definitions/instances must survive all these anomalies.
    definitions += "<Symdef uuid='" + Id(1, 10) + "'><ChildList><Geometry3D fileName='missing.3ds'/></ChildList></Symdef>";
    for (int i = 0; i < 2; ++i)
      definitions += "<Symdef uuid='" + Id(1, 11) + "'><ChildList><Geometry3D fileName='model0.3ds'/></ChildList></Symdef>";
    definitions += "<Symdef uuid='" + Id(1, 12) + "'><ChildList>" +
        Symbol(Id(1, 12), Id(1, 12), 0) + "</ChildList></Symdef>";
    nodes += "<SceneObject uuid='" + Id(2, 50) + "'><Geometries>" +
        Symbol("malformed", Id(1, 99), 0) + "<Symbol uuid='" + Id(3, 1) +
        "' symdef='" + Id(1, 0) + "'><Matrix>invalid</Matrix></Symbol>"
        "<Symbol symdef='" + Id(1, 0) + "'/></Geometries><FixtureID>Invalid</FixtureID></SceneObject>";
    nodes += "<Truss uuid='" + Id(2, 51) + "'><Geometries>" +
        Symbol(Id(3, 2), Id(1, 99), 0) + "</Geometries><FixtureID>Missing</FixtureID></Truss>";
  }
  entries.insert(entries.begin(), {"GeneralSceneDescription.xml",
      "<GeneralSceneDescription verMajor='1' verMinor='5' provider='Synthetic' providerVersion='1'>"
      "<Scene><AUXData>" + definitions + "</AUXData><Layers><Layer uuid='" + Id(4, 0) +
      "' name='Stage'><ChildList><GroupObject uuid='" + Id(5, 0) + "'>" + XmlMatrix(10) +
      "<ChildList><GroupObject uuid='" + Id(5, 1) + "'>" + XmlMatrix(20) +
      "<ChildList>" + nodes + "</ChildList></GroupObject></ChildList></GroupObject>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>"});
  return entries;
}
MvrScene Import(const fs::path &path) {
  MvrImporter importer;
  MvrImportResult result;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;
  assert(importer.ImportFromFile(path.string(), result, MvrImportMode::ParseOnly, options));
  return std::move(result.scene);
}
std::string Bytes(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  assert(in);
  return {std::istreambuf_iterator<char>(in), {}};
}
fs::path Resource(const MvrScene &scene, const std::string &file) {
  return mvr::ResolveSymdefResource(scene, file);
}
// This is also the CPU geometry loader used before renderer mesh upload.
void CheckMesh(const MvrScene &scene, const std::string &file) {
  assert(!file.empty());
  Mesh mesh;
  std::string error;
  assert(Load3DS(Resource(scene, file).string(), mesh, true, &error, false));
  assert(mesh.vertices.size() >= 9 && mesh.indices.size() >= 3);
}
std::map<std::string, std::string> Signature(const MvrScene &scene, bool meshes = true) {
  std::map<std::string, std::string> result;
  std::set<std::string> instances, occupied;
  const auto reserve = [&](const auto &values) {
    for (const auto &[id, value] : values) assert(occupied.insert(id).second);
  };
  reserve(scene.symdefs); reserve(scene.trusses); reserve(scene.sceneObjects);
  reserve(scene.fixtures); reserve(scene.supports); reserve(scene.layers);
  reserve(scene.groupObjects); reserve(scene.classes); reserve(scene.positions);
  const auto instance = [&](const std::string &id, const std::string &target) {
    assert(!id.empty() && !occupied.contains(id));
    assert(scene.symdefs.contains(target));
    assert(instances.insert(id).second);
  };
  const auto transform = [](const Matrix &m) { return mvr::FormatCanonicalMatrix(m, std::nullopt); };
  const auto node = [&](const auto &value) {
    return value.parentGroupUuid + ":" + value.layerUuid + ":" + transform(value.transform);
  };
  for (const auto &[id, definition] : scene.symdefs) {
    auto &value = result["definition:" + id];
    for (const auto &geometry : definition.geometries) {
      value += transform(geometry.transform) + Bytes(Resource(scene, geometry.file));
      if (meshes) CheckMesh(scene, geometry.file);
    }
    for (const auto &symbol : definition.symbols) {
      instance(symbol.uuid, symbol.symdef);
      value += symbol.uuid + symbol.symdef + transform(symbol.transform);
    }
  }
  for (const auto &[id, truss] : scene.trusses) {
    auto &value = result["truss:" + id];
    value = node(truss);
    if (truss.sourceRepresentation == Truss::GeometryRepresentation::SymbolSymdef) {
      instance(truss.sourceSymbolUuid, truss.sourceSymdefUuid);
      assert(mvr::SourceOwnsGeometry(scene, id));
      assert(!truss.symbolFile.empty());
      value += truss.sourceSymbolUuid + truss.sourceSymdefUuid + transform(truss.sourceSymbolMatrix);
    }
    if (meshes && !truss.symbolFile.empty()) CheckMesh(scene, truss.symbolFile);
    if (!truss.symbolFile.empty()) value += Bytes(Resource(scene, truss.symbolFile));
  }
  for (const auto &[id, object] : scene.sceneObjects) {
    auto &value = result["object:" + id];
    value = node(object);
    std::set<std::string> seen;
    for (const auto &geometry : object.geometries) {
      if (!geometry.sourceSymbolUuid.empty() && seen.insert(geometry.sourceSymbolUuid).second) {
        instance(geometry.sourceSymbolUuid, geometry.sourceSymdefUuid);
        assert(mvr::SourceOwnsGeometry(scene, id));
      }
      value += geometry.sourceSymbolUuid + geometry.sourceSymdefUuid +
          transform(geometry.localTransform) + Bytes(Resource(scene, geometry.modelFile));
      if (meshes) CheckMesh(scene, geometry.modelFile);
    }
  }
  for (const auto &[id, group] : scene.groupObjects) result["group:" + id] = node(group);
  return result;
}
struct ArchiveState {
  std::map<std::string, int> counts;
  std::multiset<std::string> resources;
  std::map<std::string, std::string> instances;
  int findings = 0;
};
ArchiveState Inspect(const fs::path &path, bool canonical) {
  ArchiveState result;
  std::set<std::string> names, definitions, ids;
  std::map<std::string, std::string> entries;
  for (const auto &entry : tests::zip::ReadEntries(path)) {
    assert(names.insert(entry.name).second);
    entries[entry.name] = entry.payload;
    if (fs::path(entry.name).extension() == ".3ds") result.resources.insert(entry.payload);
  }
  tinyxml2::XMLDocument xml;
  assert(xml.Parse(entries.at("GeneralSceneDescription.xml").c_str()) == tinyxml2::XML_SUCCESS);
  if (canonical) assert(xml.RootElement()->IntAttribute("verMinor") == 6);
  const auto visit = [&](auto &&self, tinyxml2::XMLElement *element) -> void {
    const std::string kind = element->Name();
    if (kind == "UserData") {
      const auto findings = [&](auto &&walk, tinyxml2::XMLElement *part) -> void {
        if (std::string(part->Name()) == "Finding") ++result.findings;
        for (auto *child = part->FirstChildElement(); child; child = child->NextSiblingElement())
          walk(walk, child);
      };
      findings(findings, element);
      return;
    }
    if (kind == "Finding") ++result.findings;
    if (kind == "Symdef") definitions.insert(element->Attribute("uuid"));
    if (kind != "UserData") {
      if (canonical && element->Attribute("uuid")) assert(ids.insert(element->Attribute("uuid")).second);
      if (kind == "Symbol") {
        result.instances[element->Attribute("uuid") ? element->Attribute("uuid") : ""] =
            element->Attribute("symdef") ? element->Attribute("symdef") : "";
      }
      if (kind == "Geometry3D" && canonical) assert(entries.contains(element->Attribute("fileName")));
      ++result.counts[kind];
    }
    for (auto *child = element->FirstChildElement(); child; child = child->NextSiblingElement())
      self(self, child);
  };
  visit(visit, xml.RootElement());
  if (canonical)
    for (const auto &[id, target] : result.instances) assert(definitions.contains(target));
  return result;
}
void Compare(const ArchiveState &expected, const ArchiveState &actual) {
  for (const auto *kind : {"Truss", "SceneObject", "Fixture", "Symdef", "Symbol", "Geometry3D"}) {
    const auto count = [&](const auto &state) { auto it = state.counts.find(kind); return it == state.counts.end() ? 0 : it->second; };
    assert(count(expected) == count(actual));
  }
  assert(expected.resources == actual.resources);
}
} // namespace

int RunSymbolCollisionRegression(int argc, char **argv) {
  const bool external = std::string(argv[1]) == "--symbol-file";
  const bool invalid = std::string(argv[1]) == "--symbol-invalid";
  const auto directory = argc > 3 ? fs::path(argv[3]) : fs::temp_directory_path() /
      (invalid ? "perastage-symbol-invalid" : "perastage-symbol-collisions");
  fs::create_directories(directory);
  const auto source = external ? fs::path(argv[2]) : directory / "source.mvr";
  if (!external) {
    std::string error;
    assert(tests::archive::WriteStoredZipWithRawNames(source, BuildCollisionFixture(invalid), error));
  }
  const auto originalBytes = Bytes(source);
  auto imported = Import(source);
  if (invalid) {
    assert(!imported.preservationIssues.empty());
    assert(imported.symdefs.size() == 8); // Six valid, one unavailable, one cyclic.
    assert(!imported.symdefs.contains(Id(1, 11)));
    assert(!mvr::CheckMvrSymdefs(imported).empty());
    for (const auto &geometry : imported.sceneObjects.at(Id(2, 50)).geometries) {
      assert(!geometry.sourceSymbolUuid.empty());
      assert(imported.symdefs.contains(geometry.sourceSymdefUuid));
    }
  } else if (!external) {
    assert(imported.symdefs.size() == 6 && imported.trusses.size() == 32 && imported.sceneObjects.size() == 2);
    for (int i = 0; i < 32; ++i) {
      const auto &truss = imported.trusses.at(Id(2, i));
      assert(truss.sourceRepresentation == Truss::GeometryRepresentation::SymbolSymdef);
      assert(truss.sourceSymdefUuid == Id(1, i % 6));
      assert(truss.transform.o[0] == 30 + i + 5 + (i % 6) + 1);
      assert(truss.parentGroupUuid == Id(5, 1));
    }
    for (int i = 32; i < 34; ++i) {
      const auto &object = imported.sceneObjects.at(Id(2, i));
      assert(object.transform.o[0] == 30 + i);
      assert(object.geometries.size() == 1);
      assert(object.geometries.front().sourceSymdefUuid == Id(1, i % 6));
      assert(object.geometries.front().localTransform.o[0] == 5 + (i % 6) + 1);
    }
  }
  const auto expected = Inspect(source, false);
  const auto first = directory / "canonical.mvr";
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(imported, first.string()));
  const auto canonical = Inspect(first, true);
  if (!invalid) {
    Compare(expected, canonical);
    const auto signature = Signature(imported);
    assert(Signature(Import(source)) == signature);
    assert(Signature(Import(first)) == signature);
  } else {
    assert(canonical.resources.size() == 6);
    assert(canonical.counts.at("Truss") == 33 && canonical.counts.at("Symdef") == 8);
    assert(canonical.findings > 0);
    bool disclosed = false;
    for (const auto &finding : exporter.GetExportDiagnostics())
      disclosed |= finding.impact == MvrExportDiagnosticImpact::DataOmitted &&
          finding.technicalDetail.find("no native model geometry is available") != std::string::npos;
    assert(disclosed);
  }
  auto &config = ConfigManager::Get();
  config.Reset();
  config.GetScene() = Import(first);
  const auto signature = Signature(config.GetScene());
  for (int cycle = 0; cycle < 3; ++cycle) {
    const auto project = directory / ("cycle" + std::to_string(cycle) + ".pstg");
    assert(config.SaveProject(project.string()));
    std::string embedded;
    assert(tests::zip::ReadEntry(project, "scene.mvr", &embedded));
    const auto sceneFile = directory / ("project-scene" + std::to_string(cycle) + ".mvr");
    std::ofstream(sceneFile, std::ios::binary).write(embedded.data(), embedded.size());
    const auto saved = Inspect(sceneFile, true);
    Compare(canonical, saved);
    assert(saved.instances == canonical.instances && saved.findings == canonical.findings);
    assert(config.LoadProject(project.string()));
    assert(Signature(config.GetScene()) == signature);
    const auto output = directory / ("reopened" + std::to_string(cycle) + ".mvr");
    assert(exporter.ExportCanonicalSnapshotToFile(config.GetScene(), output.string()));
    const auto reopened = Inspect(output, true);
    Compare(canonical, reopened);
    assert(reopened.instances == canonical.instances && reopened.findings == canonical.findings);
  }
  assert(Bytes(source) == originalBytes);
  std::cout << "Symbol lifecycle preserved: ";
  for (const auto *kind : {"Truss", "SceneObject", "Fixture", "Symdef", "Symbol", "Geometry3D"})
    std::cout << kind << '=' << (canonical.counts.contains(kind) ? canonical.counts.at(kind) : 0) << ' ';
  std::cout << "resources=" << canonical.resources.size() << " findings=" << canonical.findings << '\n';
  config.Reset();
  return 0;
}
