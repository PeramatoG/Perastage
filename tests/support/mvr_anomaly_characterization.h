#pragma once
#include "inspection/xml_schema_validation.h"
#include "LayoutManager.h"
#include "support/zip_test_utils.h"
#include <fstream>
#include "mvr_standard_preservation.h"
#include "mvr_source_structure_service.h"
#include "mvrexporter.h"
#include "matrixutils.h"
#include <set>
#include <cassert>
#include <tinyxml2.h>
#include <iostream>
namespace tests::mvr_anomaly {
inline std::string Source(const std::string &binding = "<GDTFSpec/><GDTFMode/>",
                          int minor = 5) {
  const std::string repeated = "71ABCDEF-0000-4000-8000-000000000001";
  const std::string klass = "72000000-0000-4000-8000-000000000001";
  std::string nodes;
  for (int i = 1; i <= 3; ++i)
    nodes +=
        "<SceneObject uuid='" + repeated + "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{" +
        std::to_string(i) + ",0,0}</Matrix><Classing>" + klass +
        "</Classing><Geometries><Geometry3D "
        "fileName='71abcdef-0000-4000-8000-000000000001.glb'/></Geometries>"
        "<FixtureID>Object</FixtureID></SceneObject>";
  for (int i = 1; i <= 2; ++i)
    nodes +=
        "<Truss uuid='73000000-0000-4000-8000-00000000000" + std::to_string(i) +
        "'><Geometries><Geometry3D "
        "fileName='71abcdef-0000-4000-8000-000000000001.glb'/></Geometries>" +
        binding + "<FixtureID>Rig</FixtureID></Truss>";
  return "<GeneralSceneDescription verMajor='1' verMinor='" +
         std::to_string(minor) +
         "' "
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
// Exercise the same canonical serializer through project and standalone output.
inline void CheckRecovered(ConfigManager &cfg,
                           const std::filesystem::path &target,
                           bool validBinding = false, bool legacyCheck = true,
                           bool omittedBinding = false, bool network = false) {
  auto &scene = cfg.GetScene();
  // Recreate the retained metadata of a session imported by the previous
  // version, while leaving the already recovered editable identities intact.
  if (legacyCheck) {
    auto legacy = scene;
    std::string legacyMoved;
    for (auto &[id, object] : legacy.sceneObjects)
      if (object.transform.o[0] == 2) {
        legacyMoved = id;
        object.transform.o[0] = 2.25f;
      }
    assert(!legacyMoved.empty());
    for (auto &original : legacy.sourceStructures) {
      auto copy = std::make_shared<MvrSourceStructure>(*original);
      for (auto &node : copy->nodes)
        if (node.kind == "SceneObject") {
          node.uuid = "71abcdef-0000-4000-8000-000000000001";
          tinyxml2::XMLDocument xml;
          xml.Parse(node.xml.c_str());
          xml.RootElement()->SetAttribute("uuid", node.uuid.c_str());
          tinyxml2::XMLPrinter printer(nullptr, true);
          xml.Print(&printer);
          node.xml = printer.CStr();
        }
      for (auto &node : copy->nodes)
        if (node.kind == "Fixture") {
          tinyxml2::XMLDocument xml;
          assert(xml.Parse(node.xml.c_str()) == tinyxml2::XML_SUCCESS);
          auto *connections = xml.NewElement("Connections");
          auto *link = xml.NewElement("Connection");
          link->SetAttribute("own", "A");
          link->SetAttribute("other", "B");
          link->SetAttribute("toObject",
                             "71abcdef-0000-4000-8000-000000000001");
          connections->InsertEndChild(link);
          xml.RootElement()->InsertEndChild(connections);
          tinyxml2::XMLPrinter printer;
          xml.Print(&printer);
          node.xml = printer.CStr();
        }
      mvr::IndexSourceStructure(*copy);
      original = copy;
    }
    tinyxml2::XMLDocument raw;
    raw.Parse(Source().c_str());
    MvrScene evidence;
    mvr::RecordStandardPreservationRisks(*raw.RootElement(), evidence);
    legacy.preservationIssues = evidence.preservationIssues;
    for (auto *truss = raw.RootElement()
                           ->FirstChildElement("Scene")
                           ->FirstChildElement("Layers")
                           ->FirstChildElement("Layer")
                           ->FirstChildElement("ChildList")
                           ->FirstChildElement("Truss");
         truss; truss = truss->NextSiblingElement("Truss")) {
      tinyxml2::XMLPrinter printer(nullptr, true);
      truss->Accept(&printer);
      legacy.preservationIssues.push_back(
          {"Truss", truss->Attribute("uuid"), "Truss/GDTFMode", printer.CStr(),
           "MVR 1.6 Table 28 requires an authored mode with GDTFSpec.", ""});
    }
    assert(!mvr::CheckSourceStructure(legacy).empty());
    MvrExporter repair;
    std::vector<uint8_t> recovered;
    assert(repair.ExportCanonicalSnapshotToBuffer(legacy, recovered) &&
           !recovered.empty());
    const auto oldSessionOutput =
        target.parent_path() / "old-session-export.mvr";
    {
      std::ofstream file(oldSessionOutput, std::ios::binary);
      file.write(reinterpret_cast<const char *>(recovered.data()),
                 recovered.size());
    }
    std::string recoveredXml;
    assert(tests::zip::ReadEntry(oldSessionOutput,
                                 "GeneralSceneDescription.xml", &recoveredXml));
    tinyxml2::XMLDocument projection;
    assert(projection.Parse(recoveredXml.c_str()) == tinyxml2::XML_SUCCESS);
    bool retainedEdit = false;
    const auto findEdit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
      if (std::string(node->Name()) == "SceneObject" &&
          node->Attribute("uuid") &&
          std::string(node->Attribute("uuid")) == legacyMoved) {
        Matrix matrix;
        assert(MatrixUtils::ParseMatrix(
            node->FirstChildElement("Matrix")->GetText(), matrix));
        retainedEdit = matrix.o[0] == 2.25f;
      }
      for (auto *child = node->FirstChildElement(); child;
           child = child->NextSiblingElement())
        self(self, child);
    };
    findEdit(findEdit, projection.RootElement()->FirstChildElement("Scene"));
    const auto checkReference = [&](auto &&self,
                                    tinyxml2::XMLElement *node) -> void {
      assert(std::string(node->Name()) != "Connection");
      for (auto *child = node->FirstChildElement(); child;
           child = child->NextSiblingElement())
        self(self, child);
    };
    checkReference(checkReference,
                   projection.RootElement()->FirstChildElement("Scene"));
    assert(std::any_of(
        repair.GetExportDiagnostics().begin(),
        repair.GetExportDiagnostics().end(), [](const auto &finding) {
          return finding.impact == MvrExportDiagnosticImpact::DataOmitted &&
                 finding.technicalDetail.find("ambiguous") != std::string::npos;
        }));
    assert(retainedEdit &&
           legacy.sceneObjects.at(legacyMoved).transform.o[0] == 2.25f);
    int reassigned = 0;
    for (const auto &finding : repair.GetExportDiagnostics())
      reassigned += finding.code == MvrExportDiagnosticCode::IdentityReassigned;
    assert(reassigned == 2 && !mvr::CheckSourceStructure(legacy).empty());
  }

  assert(scene.sceneObjects.size() == 3 && scene.trusses.size() == 2 &&
         scene.fixtures.size() == 1);
  std::set<float> positions;
  std::set<std::string> models;
  std::string moved;
  for (auto &[id, object] : scene.sceneObjects) {
    positions.insert(object.transform.o[0]);
    models.insert(object.geometries.at(0).modelFile);
    assert(object.classing == "72000000-0000-4000-8000-000000000001");
    if (object.transform.o[0] == 2) {
      moved = id;
      object.transform.o[0] += 0.5f;
    }
  }
  assert((positions == std::set<float>{1, 2, 3}) && models.size() == 1);
  assert(mvr::CheckSourceStructure(scene).empty());
  scene.fixtures.begin()->second.instanceName = "Edited fixture";
  layouts::LayoutDefinition layout;
  layout.name = "Recovered session";
  layouts::LayoutTextDefinition note;
  note.id = 1;
  note.text = "Edited layout";
  note.frame = {1, 2, 100, 50};
  layout.textViews.push_back(note);
  assert(layouts::LayoutManager::Get().AddLayout(layout));
  const auto output = target.parent_path() / "canonical.mvr";
  const auto compare = [&] {
    MvrExporter exporter;
    assert(exporter.ExportCanonicalSnapshotToFile(cfg.GetScene(),
                                                  output.string()));
    if (omittedBinding &&
        !cfg.GetScene().trusses.begin()->second.gdtfSpec.empty()) {
      int omissions = 0;
      for (const auto &finding : exporter.GetExportDiagnostics()) {
        assert(finding.severity != MvrExportDiagnosticSeverity::Error);
        if (finding.impact == MvrExportDiagnosticImpact::DataOmitted) {
          ++omissions;
          assert(finding.objectType == "Truss");
          assert(
              finding.resourceName == "Truss/GDTFSpec,GDTFMode" ||
              (network && finding.resourceName == "Truss/Addresses/Network"));
          assert(finding.technicalDetail.find(
                     finding.resourceName == "Truss/Addresses/Network"
                         ? "192.0.2.1"
                         : "fixture.gdtf") != std::string::npos);
        }
      }
      assert(omissions == (network ? 4 : 2));
    }
    std::string embedded;
    assert(tests::zip::ReadEntry(target, "scene.mvr", &embedded));
    const auto embeddedPath = target.parent_path() / "embedded.mvr";
    {
      std::ofstream file(embeddedPath, std::ios::binary);
      file << embedded;
    }
    const auto first = tests::zip::ReadEntries(output),
               second = tests::zip::ReadEntries(embeddedPath);
    assert(first.size() == second.size());
    for (size_t i = 0; i < first.size(); ++i)
      assert(first[i].name == second[i].name &&
             first[i].payload == second[i].payload);
    int models = 0;
    const auto expected = tests::gdtf::BuildMinimalValidGlb();
    for (const auto &entry : first)
      if (entry.name.ends_with(".glb")) {
        ++models;
        assert(entry.payload.size() == expected.size());
        assert(std::equal(entry.payload.begin(), entry.payload.end(),
                          expected.begin(), [](auto left, auto right) {
                            return static_cast<unsigned char>(left) ==
                                   static_cast<unsigned char>(right);
                          }));
        assert(
            entry.name.ends_with("71abcdef-0000-4000-8000-000000000001.glb"));
      }
    assert(models == 1);
    std::string xml;
    assert(tests::zip::ReadEntry(output, "GeneralSceneDescription.xml", &xml));
    tinyxml2::XMLDocument document;
    assert(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
    if (auto *data = document.RootElement()->FirstChildElement("UserData"))
      document.RootElement()->DeleteChild(data);
    std::set<std::string> ids;
    int objects = 0;
    const auto visit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
      if (auto *id = node->Attribute("uuid"))
        assert(ids.insert(id).second);
      if (std::string(node->Name()) == "SceneObject") {
        ++objects;
        assert(std::string(node->FirstChildElement("Classing")->GetText()) ==
               "72000000-0000-4000-8000-000000000001");
      }
      if (std::string(node->Name()) == "Truss") {
        assert(bool(node->FirstChildElement("GDTFSpec")) == validBinding);
        assert(bool(node->FirstChildElement("GDTFMode")) == validBinding);
        if (network) {
          const auto *addresses = node->FirstChildElement("Addresses");
          assert(addresses && addresses->FirstChildElement("Address") &&
                 !addresses->FirstChildElement("Network"));
          assert(
              std::string(addresses->FirstChildElement("Address")->GetText()) ==
              "1");
        }
        if (validBinding)
          assert(std::string(node->FirstChildElement("GDTFMode")->GetText()) ==
                 "Mode");
      }
      for (auto *child = node->FirstChildElement(); child;
           child = child->NextSiblingElement())
        self(self, child);
    };
    visit(visit, document.RootElement());
    assert(objects == 3);
    tinyxml2::XMLPrinter printer;
    document.Print(&printer);
    const auto validation = perastage::inspection::ValidateXmlAgainstSchema(
        printer.CStr(), perastage::inspection::Mvr16Schema());
    assert(validation.schema.status ==
           perastage::inspection::ValidationStatus::Valid);
  };
  assert(cfg.SaveProject(target.string()));
  compare();
  cfg.Reset();
  assert(cfg.LoadProject(target.string()));
  assert(cfg.GetScene().sceneObjects.size() == 3 &&
         cfg.GetScene().sceneObjects.at(moved).transform.o[0] == 2.5f);
  assert(cfg.GetScene().fixtures.begin()->second.instanceName ==
         "Edited fixture");
  bool found = false;
  for (const auto &page : layouts::LayoutManager::Get().GetLayouts().Items())
    if (page.name == layout.name)
      found = page.textViews.at(0).text == note.text;
  assert(found);
  assert(cfg.SaveProject(target.string()));
  compare();
}
} // namespace tests::mvr_anomaly
