#pragma once
#include <thread>
#include <algorithm>
#include <chrono>
#include "support/mvr_anomaly_characterization.h"
#include "support/archive_entry_test_utils.h"
#include "mvr_identifier_preservation.h"
namespace tests::mvr_publication {
inline void Replace(std::string &text, const std::string &from,
                    const std::string &to) {
  size_t at = 0;
  while ((at = text.find(from, at)) != std::string::npos) {
    text.replace(at, from.size(), to);
    at += to.size();
  }
}
inline std::string Source(const std::string &mode) {
  auto source = mvr_anomaly::Source();
  if (mode == "range")
    Replace(source, "<FixtureID>1</FixtureID>",
            "<FixtureID>1</FixtureID><FixtureIDNumeric>922337203685477580800</"
            "FixtureIDNumeric>");
  if (mode == "field")
    Replace(source, "<FixtureID>1</FixtureID>",
            "<CastShadow>true</CastShadow><FixtureID>1</"
            "FixtureID><Alignments><Alignment future='x'/></Alignments>");
  if (mode == "class")
    Replace(source, "<Classing>72000000-0000-4000-8000-000000000001</Classing>",
            "<Classing>72000000-0000-4000-8000-000000000099</Classing>");
  if (mode == "focus")
    Replace(source, "<FixtureID>1</FixtureID>",
            "<Focus>72000000-0000-4000-8000-000000000099</Focus><FixtureID>1</"
            "FixtureID>");
  if (mode == "connection")
    Replace(
        source, "<FixtureID>1</FixtureID>",
        "<FixtureID>1</FixtureID><Connections><Connection own='A' other='B' "
        "toObject='72000000-0000-4000-8000-000000000099'/></Connections>");
  if (mode == "model")
    Replace(source, "71abcdef-0000-4000-8000-000000000001.glb",
            "unavailable.glb");
  if (mode == "texture")
    Replace(source, "71abcdef-0000-4000-8000-000000000001.glb", "linked.gltf");
  if (mode == "gdtf")
    Replace(source, "<GDTFSpec>fixture.gdtf</GDTFSpec>",
            "<GDTFSpec>unavailable.gdtf</GDTFSpec>");
  if (mode == "mode")
    Replace(source, "<GDTFMode>Mode</GDTFMode>",
            "<GDTFMode>Unknown</GDTFMode>");
  if (mode == "matrix")
    Replace(source, "name='Original'>",
            "name='Original'><Matrix>invalid</Matrix>");
  if (mode == "addresses")
    Replace(source, "<FixtureID>1</FixtureID>",
            "<FixtureID>1</FixtureID><Addresses><Address "
            "break='0'>1</Address><Address break='+00'>513</Address><Address "
            "break='7'>1025</Address><Network geometry='unknown' "
            "ipv4='bad'/></Addresses>");
  if (mode == "range-unit")
    Replace(source, "<UnitNumber>1</UnitNumber>",
            "<UnitNumber>922337203685477580800</UnitNumber>");
  if (mode == "range-custom")
    Replace(source, "<UnitNumber>1</UnitNumber>",
            "<UnitNumber>1</UnitNumber><CustomIdType>922337203685477580800</"
            "CustomIdType><CustomId>922337203685477580801</CustomId>");
  if (mode == "source-media")
    Replace(
        source, "</ChildList></Layer>",
        "<VideoScreen uuid='74000000-0000-4000-8000-000000000001' "
        "name='Screen'><Geometries/><Sources><Source linkedGeometry='screen' "
        "type='File'>unavailable.bin</Source></Sources><FixtureID>Screen</"
        "FixtureID><FixtureIDNumeric>7</FixtureIDNumeric></VideoScreen></"
        "ChildList></Layer>");
  if (mode == "definition")
    Replace(
        source, "</AUXData>",
        "<Symdef "
        "uuid='75000000-0000-4000-8000-000000000001'><ChildList><Geometry3D "
        "fileName='unavailable.glb'/></ChildList></Symdef></AUXData>");
  if (mode == "symbol-cycle")
    Replace(source, "</AUXData>",
            "<Symdef "
            "uuid='75000000-0000-4000-8000-000000000001'><ChildList><Symbol "
            "uuid='76000000-0000-4000-8000-000000000001' "
            "symdef='75000000-0000-4000-8000-000000000001'/></ChildList></"
            "Symdef></AUXData>");
  if (mode == "duplicate-symbol")
    Replace(
        source, "</AUXData>",
        "<Symdef "
        "uuid='75000000-0000-4000-8000-000000000001'><ChildList/></"
        "Symdef><Symdef "
        "uuid='75000000-0000-4000-8000-000000000002'><ChildList><Symbol "
        "uuid='76000000-0000-4000-8000-000000000001' "
        "symdef='75000000-0000-4000-8000-000000000001'/><Symbol "
        "uuid='76000000-0000-4000-8000-000000000001' "
        "symdef='75000000-0000-4000-8000-000000000001'><Matrix>{1,0,0}{0,1,0}{"
        "0,0,1}{3,0,0}</Matrix></Symbol></ChildList></Symdef></AUXData>");
  if (mode == "source-binding")
    Replace(source, "<FixtureID>Object</FixtureID>",
            "<GDTFSpec>unavailable.gdtf</GDTFSpec><GDTFMode>Mode</"
            "GDTFMode><FixtureID>Object</FixtureID>");
  if (mode == "source-gobo")
    Replace(
        source, "<UnitNumber>1</UnitNumber>",
        "<UnitNumber>1</UnitNumber><Gobo rotation='0'>unavailable.png</Gobo>");
  if (mode == "truss-edits" || mode == "truss-zero" || mode == "gdtf-content")
    Replace(source, "<GDTFSpec/><GDTFMode/>",
            "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode>");
  if (mode == "class-invalid")
    Replace(source, "72000000-0000-4000-8000-000000000001", "invalid-class-id");
  if (mode == "class-duplicate")
    Replace(source, "</AUXData>",
            "<Class uuid='72000000-0000-4000-8000-000000000001' name='Second "
            "class'/></AUXData>");
  if (mode == "object-symbol") {
    Replace(
        source, "</AUXData>",
        "<Symdef "
        "uuid='75000000-0000-4000-8000-000000000001'><ChildList><Geometry3D "
        "fileName='71abcdef-0000-4000-8000-000000000001.glb'/></ChildList></"
        "Symdef></AUXData>");
    Replace(
        source,
        "<Geometry3D "
        "fileName='71abcdef-0000-4000-8000-000000000001.glb'/></Geometries>",
        "<Symbol uuid='76000000-0000-4000-8000-000000000001' "
        "symdef='75000000-0000-4000-8000-000000000001'/><Symbol "
        "uuid='76000000-0000-4000-8000-000000000001' "
        "symdef='75000000-0000-4000-8000-000000000001'><Matrix>{1,0,0}{0,1,0}{"
        "0,0,1}{7,0,0}</Matrix></Symbol></Geometries>");
  }
  if (mode == "color")
    Replace(source, "<FixtureID>1</FixtureID>",
            "<Color>invalid</Color><FixtureID>1</FixtureID>");
  if (mode == "position")
    Replace(source, "<FixtureID>1</FixtureID>",
            "<Position>72000000-0000-4000-8000-000000000099</"
            "Position><FixtureID>1</FixtureID>");
  if (mode == "affine-source") {
    Replace(
        source, "<ChildList><Fixture",
        "<Matrix>{0,1,0}{-1,0,0}{0,0,1}{10,20,30}</Matrix><ChildList><Fixture");
    Replace(
        source, "</ChildList></Layer>",
        "<VideoScreen uuid='74000000-0000-4000-8000-000000000001' "
        "name='Screen'><Matrix>{1,0,0}{0,1,0}{0,0,1}{2,3,4}</"
        "Matrix><Geometries/><FixtureID>Screen</FixtureID><FixtureIDNumeric>7</"
        "FixtureIDNumeric></VideoScreen></ChildList></Layer>");
  }
  if (mode == "ambiguous-connection" || mode == "wrong-connection")
    Replace(source, "<FixtureID>1</FixtureID>",
            "<FixtureID>1</FixtureID><Connections><Connection own='A' "
            "other='B' toObject='" +
                std::string(mode == "ambiguous-connection"
                                ? "71ABCDEF-0000-4000-8000-000000000001"
                                : "72000000-0000-4000-8000-000000000001") +
                "'/></Connections>");
  return source;
}
inline void Check(ConfigManager &cfg, const std::filesystem::path &target,
                  const std::string &mode) {
  auto &scene = cfg.GetScene();
  assert(scene.fixtures.size() == 1 && scene.sceneObjects.size() == 3 &&
         scene.trusses.size() == 2);
  const auto fixtureId = scene.fixtures.begin()->first;
  scene.fixtures.at(fixtureId).instanceName = "Edited fixture";
  scene.fixtures.at(fixtureId).transform.o[0] = 42.5f;
  layouts::LayoutDefinition page;
  page.name = "Publication recovery";
  layouts::LayoutTextDefinition text;
  text.id = 42;
  text.text = "Preserved edited layout";
  text.frame = {1, 2, 100, 30};
  page.textViews.push_back(text);
  assert(layouts::LayoutManager::Get().AddLayout(page));
  std::string editedTruss;
  if (mode == "truss-edits" || mode == "truss-zero") {
    auto &truss = scene.trusses.begin()->second;
    editedTruss = truss.uuid;
    truss.lengthMm = 1234.5678f;
    truss.widthMm = 234.5f;
    truss.heightMm = 345.6f;
    truss.weightKg = 456.7f;
    truss.manufacturer = "Edited maker";
    if (mode == "truss-zero") {
      truss.lengthMm = truss.widthMm = truss.heightMm = truss.weightKg = 0;
      truss.manufacturer.clear();
      truss.model.clear();
      truss.gdtfDescription.clear();
      truss.crossSection.clear();
    }
  }
  if (mode == "power-edits")
    scene.fixtures.at(fixtureId).SetPlanningPowerOverride(321.125f);
  if (mode == "power-physical") {
    auto &f = scene.fixtures.at(fixtureId);
    f.powerConsumptionW = 321.125f;
    f.powerConsumptionWPresent = true;
    f.physicalPropertiesDirty = true;
    f.physicalPropertiesSource = FixturePhysicalPropertiesSource::Manual;
  }
  symbols::ProjectFixtureSymbolBundle customSymbol;
  if (mode == "gdtf") {
    customSymbol.kind = symbols::ProjectFixtureSymbolKind::UserOverride;
    customSymbol.generatorVersion = "publication-test";
    for (auto &view : customSymbol.views)
      view.svg = "<svg viewBox='0 0 10 10'><path d='M0 0L10 10'/></svg>";
    std::string error;
    assert(cfg.GetProjectFixtureSymbols().ApplyToFixtures(scene, {fixtureId},
                                                          customSymbol, error));
  }
  if (mode == "acquisition")
    scene.preservationIssues.push_back(
        {"GDTFSpec", fixtureId, "Fixture/GDTFSpec",
         scene.fixtures.at(fixtureId).gdtfSpec,
         "Source resource acquisition failed during merge.",
         "Restore the source definition.",
         MvrPreservationIssueKind::ResourceFailure});
  const auto sourceIssues = scene.preservationIssues;
  if (mode == "gdtf" || mode == "mode" || mode == "gdtf-sections" || mode == "acquisition") {
    const auto spec = scene.fixtures.at(fixtureId).gdtfSpec;
    const auto selectedMode = scene.fixtures.at(fixtureId).gdtfMode;
    { std::ofstream previous(target, std::ios::binary); previous << "previous publication"; }
    assert(!cfg.SaveProject(target.string()));
    const auto previous = archive::ReadBinaryFile(target.string());
    assert(std::string(previous.begin(), previous.end()) == "previous publication");
    assert(cfg.GetScene().fixtures.at(fixtureId).gdtfSpec == spec);
    assert(cfg.GetScene().fixtures.at(fixtureId).gdtfMode == selectedMode);
    assert(cfg.GetScene().preservationIssues == sourceIssues);
    MvrExporter exporter;
    std::vector<std::uint8_t> bytes;
    assert(!exporter.ExportToBuffer(bytes) && bytes.empty());
    assert(std::any_of(exporter.GetExportDiagnostics().begin(), exporter.GetExportDiagnostics().end(),
        [](const auto &finding) { return finding.severity == MvrExportDiagnosticSeverity::Error; }));
    return;
  }
  assert(cfg.SaveProject(target.string()));
  assert(scene.preservationIssues ==
         sourceIssues); // Export exceptions never edit the open scene.
  if (mode == "gdtf-content" || mode == "power-physical")
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  const auto output = target.parent_path() / "publication.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(output.string()));
  bool warning = false;
  for (const auto &finding : exporter.GetExportDiagnostics()) {
    assert(finding.severity != MvrExportDiagnosticSeverity::Error);
    warning |= finding.severity == MvrExportDiagnosticSeverity::Warning;
  }
  // Object Symbol occurrence recovery now happens during import. Its retained
  // disclosure is checked below; unchanged export needs no new repair warning.
  if (!mode.starts_with("range") && mode != "power-edits" && mode != "object-symbol")
    assert(warning);
  std::string embedded;
  assert(zip::ReadEntry(target, "scene.mvr", &embedded));
  const auto embeddedPath = target.parent_path() / "publication-embedded.mvr";
  {
    std::ofstream file(embeddedPath, std::ios::binary);
    file << embedded;
  }
  const auto standalone = zip::ReadEntries(output),
             inside = zip::ReadEntries(embeddedPath);
  assert(standalone.size() == inside.size());
  for (size_t i = 0; i < standalone.size(); ++i)
    assert(standalone[i].name == inside[i].name &&
           standalone[i].payload == inside[i].payload);
  std::string xml;
  assert(zip::ReadEntry(output, "GeneralSceneDescription.xml", &xml));
  tinyxml2::XMLDocument document;
  assert(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  auto *root = document.RootElement();
  bool evidence = false;
  if (auto *data = root->FirstChildElement("UserData")) {
    for (auto *block = data->FirstChildElement("Data"); block;
         block = block->NextSiblingElement("Data"))
      evidence |= block->Attribute("provider") &&
                  std::string(block->Attribute("provider")) ==
                      "org.perastage.canonical-recovery";
    root->DeleteChild(data);
  }
  if (!mode.starts_with("range") && mode != "power-edits")
    assert(evidence);
  int fixtures = 0, objects = 0;
  std::set<std::string> ids;
  const auto visit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
    if (auto *id = node->Attribute("uuid"))
      assert(ids.insert(id).second);
    const std::string kind = node->Name();
    if (kind == "Fixture") {
      ++fixtures;
      assert(std::string(node->Attribute("name")) == "Edited fixture");
      if (mode == "range")
        assert(std::string(
                   node->FirstChildElement("FixtureIDNumeric")->GetText()) ==
               "922337203685477580800");
      if (mode == "range-unit")
        assert(std::string(node->FirstChildElement("UnitNumber")->GetText()) ==
               "922337203685477580800");
      if (mode == "range-custom")
        assert(std::string(node->FirstChildElement("CustomId")->GetText()) ==
               "922337203685477580801");
      if (mode == "source-gobo")
        assert(!node->FirstChildElement("Gobo"));
      if (mode == "field")
        assert(!node->FirstChildElement("Alignments") &&
               std::string(node->FirstChildElement("CastShadow")->GetText()) ==
                   "true");
      if (mode == "position")
        assert(!node->FirstChildElement("Position"));
      if (mode == "focus")
        assert(!node->FirstChildElement("Focus"));
      if (mode == "connection" || mode == "ambiguous-connection" ||
          mode == "wrong-connection")
        assert(!node->FirstChildElement("Connections") ||
               !node->FirstChildElement("Connections")
                    ->FirstChildElement("Connection"));
      if (mode == "addresses") {
        auto *addresses = node->FirstChildElement("Addresses");
        assert(addresses && !addresses->FirstChildElement("Network"));
        int count = 0;
        for (auto *address = addresses->FirstChildElement("Address"); address;
             address = address->NextSiblingElement("Address"))
          ++count;
        assert(count == 2);
      }
      if (mode == "acquisition")
        assert(std::string(node->FirstChildElement("GDTFSpec")->GetText()) !=
               "fixture.gdtf");
      if (mode == "mode")
        assert(std::string(node->FirstChildElement("GDTFMode")->GetText()) ==
               "Mode");
    }
    if (kind == "SceneObject") {
      ++objects;
      if (mode == "class-invalid")
        assert(node->FirstChildElement("Classing"));
      if (mode == "class-duplicate")
        assert(!node->FirstChildElement("Classing"));
      if (mode == "class")
        assert(!node->FirstChildElement("Classing"));
      if (mode == "source-binding")
        assert(!node->FirstChildElement("GDTFSpec") &&
               !node->FirstChildElement("GDTFMode"));
      if (mode == "model" || mode == "texture")
        assert(node->FirstChildElement("Geometries") &&
               !node->FirstChildElement("Geometries")->FirstChildElement());
    }
    for (auto *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      self(self, child);
  };
  visit(visit, root->FirstChildElement("Scene"));
  assert(fixtures == 1 && objects == 3);
  if (mode == "object-symbol") {
    int symbols = 0;
    const auto count = [&](auto &&self,
                           const tinyxml2::XMLElement *element) -> void {
      if (std::string(element->Name()) == "Symbol")
        ++symbols;
      for (auto *child = element->FirstChildElement(); child;
           child = child->NextSiblingElement())
        self(self, child);
    };
    count(count, root->FirstChildElement("Scene"));
    assert(symbols == 10);
  }
  if (mode == "affine-source") {
    const auto *layers =
        root->FirstChildElement("Scene")->FirstChildElement("Layers");
    const auto *layer = layers->FirstChildElement("Layer");
    while (layer && std::string(layer->Attribute("uuid")) !=
                        "11000000-0000-4000-8000-000000000001")
      layer = layer->NextSiblingElement("Layer");
    assert(layer);
    const auto *screen =
        layer->FirstChildElement("ChildList")->FirstChildElement("VideoScreen");
    assert(screen);
    Matrix local, layerMatrix;
    assert(MatrixUtils::ParseMatrix(
        screen->FirstChildElement("Matrix")->GetText(), local));
    if (auto *matrix = layer->FirstChildElement("Matrix"))
      assert(MatrixUtils::ParseMatrix(matrix->GetText(), layerMatrix));
    const auto world = MatrixUtils::Multiply(layerMatrix, local);
    assert(world.o[0] == 7 && world.o[1] == 22 && world.o[2] == 34);
  }
  tinyxml2::XMLPrinter printer;
  document.Print(&printer);
  const auto validation = perastage::inspection::ValidateXmlAgainstSchema(
      printer.CStr(), perastage::inspection::Mvr16Schema());
  if (validation.schema.status !=
      perastage::inspection::ValidationStatus::Valid)
    for (const auto &issue : validation.schema.diagnostics)
      std::cerr << issue.message << '\n';
  assert(validation.schema.status ==
         perastage::inspection::ValidationStatus::Valid);
  cfg.Reset();
  assert(cfg.LoadProject(target.string()));
  assert(cfg.GetScene().fixtures.size() == 1 &&
         cfg.GetScene().sceneObjects.size() == 3);
  const auto &fixture = cfg.GetScene().fixtures.at(fixtureId);
  if (mode == "gdtf") {
    auto *bundle = cfg.GetProjectFixtureSymbols().FindForFixture(fixture);
    assert(bundle && *bundle == customSymbol);
  }

  assert(fixture.instanceName == "Edited fixture" &&
         fixture.transform.o[0] == 42.5f);
  bool layout = false;
  for (const auto &item : layouts::LayoutManager::Get().GetLayouts().Items())
    if (item.name == page.name)
      layout = item.textViews.at(0).text == text.text;
  assert(layout);
  if (!mode.starts_with("range") && mode != "power-edits") {
    bool retained = false;
    for (const auto &block : cfg.GetScene().opaqueUserDataBlocks)
      retained |= block.provider == "org.perastage.canonical-recovery";
    assert(retained);
  }
  if (mode == "power-edits")
    assert(fixture.planningPowerOverrideW == std::optional<float>{321.125f} &&
           fixture.powerConsumptionW == 321.125f);
  if (mode == "power-physical")
    assert(fixture.powerConsumptionW == 321.125f);
  if (mode == "truss-edits" || mode == "truss-zero") {
    for (int cycle = 0; cycle < 3; ++cycle) {
      const auto &truss = cfg.GetScene().trusses.at(editedTruss);
      if (mode == "truss-zero")
        assert(truss.lengthMm == 0 && truss.widthMm == 0 &&
               truss.heightMm == 0 && truss.weightKg == 0 &&
               truss.manufacturer.empty() && truss.model.empty());
      else
        assert(truss.lengthMm == 1234.5678f && truss.widthMm == 234.5f &&
               truss.heightMm == 345.6f && truss.weightKg == 456.7f &&
               truss.manufacturer == "Edited maker");
      assert(cfg.SaveProject(target.string()));
      cfg.Reset();
      assert(cfg.LoadProject(target.string()));
    }
  }
  assert(cfg.SaveProject(target.string()));
  MvrExporter reopened;
  assert(reopened.ExportToFile(output.string()));
}
} // namespace tests::mvr_publication
