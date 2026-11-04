#pragma once
#include "mvr_standard_preservation.h"
#include "mvr_source_structure_service.h"
#include "mvrexporter.h"
#include <set>
#include <cassert>
#include <tinyxml2.h>
#include <iostream>
namespace tests::mvr_anomaly {
inline std::string Source() {
  const std::string repeated = "71000000-0000-4000-8000-000000000001";
  const std::string klass = "72000000-0000-4000-8000-000000000001";
  std::string nodes;
  for (int i = 1; i <= 3; ++i)
    nodes += "<SceneObject uuid='" + repeated +
             "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{" + std::to_string(i) +
             ",0,0}</Matrix><Classing>" + klass +
             "</Classing><Geometries><Geometry3D "
             "fileName='shared.glb'/></Geometries>"
             "<FixtureID>Object</FixtureID></SceneObject>";
  for (int i = 1; i <= 2; ++i)
    nodes += "<Truss uuid='73000000-0000-4000-8000-00000000000" +
             std::to_string(i) +
             "'><Geometries><Geometry3D fileName='shared.glb'/></Geometries>"
             "<GDTFSpec/><GDTFMode/><FixtureID>Rig</FixtureID></Truss>";
  return "<GeneralSceneDescription verMajor='1' verMinor='5' "
         "provider='Synthetic' providerVersion='1'>"
         "<Scene><AUXData><Class uuid='" +
         klass +
         "' name='Test'/></AUXData><Layers>"
         "<Layer uuid='11000000-0000-4000-8000-000000000001'><ChildList>"
         "<Fixture uuid='51000000-0000-4000-8000-000000000001' name='Original'>"
         "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</"
         "GDTFMode><FixtureID>1</FixtureID><UnitNumber>1</UnitNumber>"
         "</Fixture>" +
         nodes +
         "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
}
// Characterization of the inspected branch, to be replaced by positive
// round-trip assertions when the recovery owner is implemented.
inline void CheckBaseline(ConfigManager &cfg,
                          const std::filesystem::path &target) {
  const auto &scene = cfg.GetScene();
  assert(scene.sceneObjects.size() == 3 && scene.trusses.size() == 2 &&
         scene.fixtures.size() == 1);
  std::set<float> positions;
  for (const auto &[id, object] : scene.sceneObjects) {
    positions.insert(object.transform.o[0]);
    assert(object.classing == "72000000-0000-4000-8000-000000000001");
  }
  assert((positions == std::set<float>{1, 2, 3}));
  const auto diagnostics = mvr::CheckStandardPreservation(scene);
  bool duplicate = false, classing = false;
  int modes = 0;
  for (const auto &d : diagnostics) {
    std::cout << int(d.code) << " | " << d.objectIdentity << " | "
              << d.resourceName << " | " << d.technicalDetail << '\n';
    duplicate |= d.technicalDetail.find("Duplicate source node identity") !=
                 std::string::npos;
    classing |= d.technicalDetail.find("Classing has an ambiguous owner") !=
                std::string::npos;
    modes += d.technicalDetail.find("Table 28 requires") != std::string::npos;
  }
  assert(duplicate && classing && modes == 2);
  assert(!mvr::CheckSourceStructure(scene).empty());
  tinyxml2::XMLDocument projected;
  assert(projected.Parse(Source().c_str()) == tinyxml2::XML_SUCCESS);
  mvr::ProjectSourceStructure(projected, scene,
                              [](const std::string &, const std::string &path,
                                 mvr::SourceResourceKind) { return path; });
  int repeated = 0;
  const auto count = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
    if (node->Attribute("uuid") && std::string(node->Attribute("uuid")) ==
                                       "71000000-0000-4000-8000-000000000001")
      ++repeated;
    for (auto *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      self(self, child);
  };
  count(count, projected.RootElement());
  assert(repeated == 3);
  std::cout
      << "Structural projection retains three ambiguous raw XML identities.\n";

  cfg.MarkDirty();
  assert(!cfg.SaveProject(target.string()) && cfg.IsDirty());
  MvrExporter exporter;
  std::vector<uint8_t> bytes;
  assert(!exporter.ExportCanonicalSnapshotToBuffer(scene, bytes) &&
         bytes.empty());
  std::cout << "Import retains all occurrences; preservation, PSTG and "
               "canonical export are blocked.\n";
}
} // namespace tests::mvr_anomaly
