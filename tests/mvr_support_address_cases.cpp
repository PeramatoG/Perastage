/* This file is part of Perastage. */
#include "mvr_support_address_cases.h"
#include "configmanager.h"
#include "fixture_network_reference.h"
#include "scene_node_operations.h"
#include "scene_clipboard.h"
#include "layer_identity.h"
#include "mvrexporter.h"
#include "mvrscenemerger.h"
#include "mvr_standard_preservation.h"
#include "inspection/xml_schema_validation.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include <cassert>
#include <fstream>
namespace fs = std::filesystem;
namespace {
constexpr auto uuid = "51000000-0000-4000-8000-000000000002";
std::string Source(const std::string &addresses) {
  return "<GeneralSceneDescription verMajor='1' verMinor='6' provider='SupportTest' providerVersion='1'>"
      "<Scene><Layers><Layer uuid='11000000-0000-4000-8000-000000000001' name='Rig'>"
      "<ChildList><Support uuid='" + std::string(uuid) + "' name='Motor'>"
      "<Matrix>{1,0,0}{0,1,0}{0,0,1}{1234.567890123,0,0}</Matrix><Geometries><Geometry3D fileName='support.glb'/></Geometries>"
      "<Function>Lighting</Function><ChainLength>0</ChainLength><GDTFSpec>fixture.gdtf</GDTFSpec>"
      "<GDTFMode>Mode</GDTFMode>" + addresses + "<FixtureID>Motor.07</FixtureID>"
      "<FixtureIDNumeric>17</FixtureIDNumeric><UnitNumber>0</UnitNumber>"
      "<CustomIdType>2</CustomIdType><CustomId>3</CustomId></Support>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
}
void Archive(const fs::path &path, const std::string &xml, const FixtureNetworkTestServices &services) {
  services.archive(path, xml);
  std::string gdtf, error;
  assert(tests::zip::ReadEntry(path, "fixture.gdtf", &gdtf));
  assert(tests::archive::WriteStoredZipWithRawNames(path, {{"GeneralSceneDescription.xml", xml},
      {"fixture.gdtf", gdtf}, {"support.glb", tests::gdtf::BuildMinimalValidGlb()}}, error));
}
void Schema(const std::string &xml) {
  using namespace perastage::inspection;
  tinyxml2::XMLDocument document;
  assert(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  if (auto *data = document.RootElement()->FirstChildElement("UserData"))
    document.RootElement()->DeleteChild(data);
  tinyxml2::XMLPrinter printer; document.Print(&printer);
  const auto validation = ValidateXmlAgainstSchema(printer.CStr(), Mvr16Schema());
  // The pinned IPv6 regex rejects normative compressed/uppercase spelling.
  assert(validation.schema.status == (xml.find("ipv6=") == std::string::npos
      ? ValidationStatus::Valid : ValidationStatus::Invalid));
}
void Export(const MvrScene &scene, const fs::path &path) {
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, path.string()));
  std::string xml;
  assert(tests::zip::ReadEntry(path, "GeneralSceneDescription.xml", &xml));
  Schema(xml);
}
std::string Bytes(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}
}
void RunSupportAddressCases(const fs::path &dir, const FixtureNetworkTestServices &services) {
  auto &config = ConfigManager::Get();
  const std::string mixed = "<Addresses><Address>00017</Address><Address break='+007'>2049</Address>"
      "<Network geometry='ethernet_1' ipv4='192.168.001.005' subnetmask='255.255.0.0'/>"
      "<Network geometry='wireless_1' dhcp='false' hostname=''/><Network geometry='" +
      std::string("51000000-0000-4000-8000-000000000001") + "' dhcp='on' hostname='Rig.Left'/></Addresses>";
  for (const auto &[name, content] : std::vector<std::pair<std::string, std::string>>{
      {"none", ""}, {"empty", "<Addresses/>"},
      {"single", "<Addresses><Address>17</Address></Addresses>"},
      {"multi", "<Addresses><Address break='0'>17</Address><Address break='19'>4097</Address></Addresses>"},
      {"only", "<Addresses><Network geometry='ethernet_1'/></Addresses>"},
      {"ipv6", "<Addresses><Network geometry='wireless_1' ipv6='2001:DB8::A'/></Addresses>"}, {"mixed", mixed}}) {
    const auto stem = "support-" + name;
    const auto source = Source(content); Schema(source);
    Archive(dir / (stem + "-source.mvr"), source, services);
    const auto scene = services.import(dir / (stem + "-source.mvr"));
    assert(scene.preservationIssues.empty());
    const auto &support = scene.supports.at(uuid);
    assert(support.fixtureIdText == "Motor.07" && support.customId == 3 && support.customIdType == 2);
    assert(support.matrixSource);
    Export(scene, dir / (stem + ".mvr"));
    std::string xml;
    assert(tests::zip::ReadEntry(dir / (stem + ".mvr"), "GeneralSceneDescription.xml", &xml)); Schema(xml);
    config.Reset(); config.GetScene() = scene;
    for (int cycle = 1; cycle <= 3; ++cycle) {
      const auto file = stem + "-cycle" + std::to_string(cycle);
      Export(config.GetScene(), dir / (file + "-standalone.mvr"));
      assert(config.SaveProject((dir / (file + ".pstg")).string()));
      config.Reset(); assert(config.LoadProject((dir / (file + ".pstg")).string()));
      assert(config.GetScene().supports.at(uuid).dmxAddresses == support.dmxAddresses);
      Export(config.GetScene(), dir / (file + ".mvr"));
    }
    if (name != "mixed") continue;
    config.Reset(); config.GetScene() = scene;
    config.PushUndoState("edit Support without changing addresses");
    config.GetScene().supports.at(uuid).name = "Edited";
    Export(config.GetScene(), dir / "support-edited.mvr");
    config.Undo(); Export(config.GetScene(), dir / "support-undo.mvr");
    config.Redo(); Export(config.GetScene(), dir / "support-redo.mvr");
    auto changed = scene;
    Fixture anchor; anchor.uuid = "51000000-0000-4000-8000-000000000004";
    anchor.gdtfSpec = support.gdtfSpec; anchor.gdtfMode = "Mode";
    layerdomain::AssignLayer(changed, anchor, support.layerUuid); changed.fixtures[anchor.uuid] = anchor;
    scene_grouping::ObjectSelection selection; selection.supports = {uuid}; selection.fixtures = {anchor.uuid};
    assert(scene_grouping::GroupSelection(changed, selection).changed);
    Export(changed, dir / "support-grouped.mvr");
    assert(scene_grouping::RemoveSelectionFromGroup(changed, selection).changed);
    layerdomain::AssignLayer(changed, changed.supports.at(uuid), layerdomain::DefaultLayerUuid());
    Export(changed, dir / "support-layer.mvr");
    auto merged = scene;
    const auto plan = mvr::AnalyzeImportedSceneMerge(merged, scene);
    mvr::ApplyImportedSceneMergeAtomically(merged, scene, plan);
    assert(merged.supports.at(plan.uuidMap.at(uuid)).dmxAddresses == support.dmxAddresses);
    Export(merged, dir / "support-merged.mvr");
    SelectionState selected; selected.SetSelectedSupports({uuid});
    scene_clipboard::Service clipboard; changed = scene;
    assert(clipboard.Capture(changed, selected, 1));
    assert(clipboard.Paste(changed, 1).changed);
    Export(changed, dir / "support-copied.mvr");
    auto candidate = support;
    auto path = fs::path(support.gdtfSpec);
    if (!path.is_absolute()) path = fs::path(scene.basePath) / path;
    assert(fixture_network::InspectNodeReferences(candidate, path));
    assert(fixture_network::RebindNode(candidate, "", path).conflict == fixture_network::Conflict::InvalidBinding);
    assert(fixture_network::RebindNode(candidate, "missing.gdtf", dir / "missing.gdtf").conflict == fixture_network::Conflict::UnavailableGdtf);
    assert(candidate.gdtfSpec == support.gdtfSpec && candidate.dmxAddresses == support.dmxAddresses);
    const auto replacement = dir / "support-replacement.gdtf";
    tests::gdtf::BuildMinimalValidFixture().WriteArchive(replacement);
    assert(fixture_network::RebindNode(candidate, replacement.string(), replacement).conflict == fixture_network::Conflict::UnknownGeometry);
    std::string error;
    assert(tests::archive::WriteStoredZipWithRawNames(replacement, {{"description.xml",
        "<GDTF><FixtureType><Geometries><WiringObject Name='ethernet_1'/><WiringObject Name='ethernet_1'/></Geometries></FixtureType></GDTF>"}}, error));
    assert(fixture_network::InspectNodeReferences(candidate, replacement).conflict == fixture_network::Conflict::AmbiguousGeometry);
    config.GetScene() = scene;
    const auto retained = dir / "support-retained.pstg";
    assert(config.SaveProject(retained.string())); const auto previous = Bytes(retained);
    for (const auto &invalid : {std::string("missing.gdtf"), replacement.string()}) {
      config.GetScene() = scene; config.GetScene().supports.at(uuid).gdtfSpec = invalid;
      assert(!config.SaveProject(retained.string())); assert(Bytes(retained) == previous);
      assert(config.GetScene().supports.at(uuid).dmxAddresses == support.dmxAddresses);
    }
  }
  for (const auto *content : {"<Addresses/><Addresses/>", "<Addresses future='x'/>",
      "<Addresses><Address future='x'>17</Address></Addresses>",
      "<Addresses><Address break='7'>1</Address><Address break='+007'>2</Address></Addresses>",
      "<Addresses><Network/></Addresses>", "<Addresses><Network geometry='ethernet_1' future='x'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1'/><Address>17</Address></Addresses>",
      "<Addresses><Network geometry='ethernet_1'><Child/></Network></Addresses>"}) {
    Archive(dir / "support-invalid.mvr", Source(content), services);
    auto invalid = services.import(dir / "support-invalid.mvr");
    assert(!invalid.preservationIssues.empty());
    MvrExporter exporter; std::vector<unsigned char> bytes;
    assert(!exporter.ExportCanonicalSnapshotToBuffer(invalid, bytes));
  }
  std::string archiveError;
  assert(tests::archive::WriteStoredZipWithRawNames(dir / "support-missing-source.mvr",
      {{"GeneralSceneDescription.xml", Source(mixed)}, {"support.glb", tests::gdtf::BuildMinimalValidGlb()}}, archiveError));
  auto missingScene = services.import(dir / "support-missing-source.mvr");
  assert(missingScene.supports.at(uuid).dmxAddresses->networks.size() == 3);
  const auto missingDiagnostics = mvr::CheckStandardPreservation(missingScene);
  assert(std::any_of(missingDiagnostics.begin(), missingDiagnostics.end(), [](const auto &diagnostic) {
    return diagnostic.code == MvrExportDiagnosticCode::ResourceMissing;
  }));
  // Conversion keeps the same standard identity, definition and numeric matrix source.
  auto conversionSource = services.source(mixed);
  size_t at = 0;
  const std::string matrix = "<Matrix>{1,0,0}{0,1,0}{0,0,1}{1234.567890123,0,0}</Matrix>";
  while ((at = conversionSource.find("<GDTFSpec>", at)) != std::string::npos) {
    conversionSource.insert(at, matrix); at += matrix.size() + 1;
  }
  services.archive(dir / "support-conversion-source.mvr", conversionSource);
  auto converted = services.import(dir / "support-conversion-source.mvr");
  constexpr auto classUuid = "61000000-0000-4000-8000-000000000001";
  converted.classes[classUuid] = MvrClass{std::string("Motors")};
  for (auto &[id, fixture] : converted.fixtures) {
    fixture.classing = classUuid; fixture.instanceName = "Motor " + id;
    fixture.fixtureIdText = "Motor." + std::to_string(fixture.fixtureId);
    fixture.customIdType = 2; fixture.customId = 3;
  }
  scene_grouping::ObjectSelection conversionSelection;
  for (const auto &[id, fixture] : converted.fixtures) conversionSelection.fixtures.push_back(id);
  assert(scene_grouping::GroupSelection(converted, conversionSelection).changed);
  Export(converted, dir / "support-conversion-before.mvr");
  config.Reset(); config.GetScene() = converted;
  config.PushUndoState("convert addressed Fixture to Support");
  for (const auto &id : conversionSelection.fixtures) {
    const auto before = converted.fixtures.at(id);
    const auto result = scene_node_operations::ConvertFixtureToSupport(config.GetScene(), id);
    assert(result.changed);
    const auto &after = config.GetScene().supports.at(id);
    assert(after.dmxAddresses == before.dmxAddresses && after.classing == before.classing);
    assert(after.parentGroupUuid == before.parentGroupUuid && after.layerUuid == before.layerUuid);
    assert(after.matrixSource && before.matrixSource);
    assert(after.matrixSource->values == before.matrixSource->values);
  }
  Export(config.GetScene(), dir / "support-converted.mvr");
  assert(config.SaveProject((dir / "support-converted.pstg").string()));
  config.Undo(); Export(config.GetScene(), dir / "support-conversion-undo.mvr");
  config.Redo(); Export(config.GetScene(), dir / "support-conversion-redo.mvr");
  config.Reset(); assert(config.LoadProject((dir / "support-converted.pstg").string()));
  Export(config.GetScene(), dir / "support-converted-reopened.mvr");
  config.Reset();
}
