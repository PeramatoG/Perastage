/* This file is part of Perastage. */
#include "configmanager.h"
#include "fixture_patch_service.h"
#include "autopatcher.h"
#include "fixture_patch_workflow.h"
#include "scene_clipboard.h"
#include "layer_identity.h"
#include "inspection/xml_schema_validation.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvrscenemerger.h"
#include "mvr_fixture_addresses.h"
#include "mvr_fixture_network_cases.h"
#include "mvr_support_address_cases.h"
#include "scene_grouping.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include <wx/init.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
namespace {
constexpr auto single = "51000000-0000-4000-8000-000000000001";
constexpr auto multi = "51000000-0000-4000-8000-000000000002";
constexpr auto nonzero = "51000000-0000-4000-8000-000000000003";
std::string FixtureXml(const char *uuid, const std::string &addresses) {
  return "<Fixture uuid='" + std::string(uuid) + "'><GDTFSpec>fixture.gdtf</GDTFSpec>"
      "<GDTFMode>Mode</GDTFMode><FixtureID>7</FixtureID><FixtureIDNumeric>7</FixtureIDNumeric>"
      "<UnitNumber>7</UnitNumber>" + addresses + "</Fixture>";
}
std::string Source(const std::string &addresses = "<Addresses><Address break='0'>1025</Address>"
    "<Address break='7'>2049</Address><Address break='19'>4097</Address>"
    "<Network geometry='ethernet_1' ipv4='192.168.001.005' subnetmask='255.255.0.0'/>"
    "<Network geometry='wireless_1' dhcp='on' hostname='Rig.Left'/>"
    "<Network geometry='51000000-0000-4000-8000-000000000001'/></Addresses>") {
  return "<GeneralSceneDescription verMajor='1' verMinor='6' provider='AddressTest' providerVersion='1'>"
      "<Scene><Layers><Layer uuid='11000000-0000-4000-8000-000000000001' name='Rig'>"
      "<Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,1200}</Matrix><ChildList>" +
      FixtureXml(single, "<Addresses><Address>17</Address></Addresses>") + FixtureXml(multi, addresses) +
      FixtureXml(nonzero, "<Addresses><Address break='7'>1025</Address></Addresses>") +
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
}
void Archive(const fs::path &path, const std::string &xml) {
  const auto fixture = path.parent_path() / "fixture.gdtf";
  auto description = tests::gdtf::BuildMinimalValidFixture().WithDmxMode("Mode", "Root").WithModelResource("body").BuildDescriptionXml();
  const auto end = description.find("</Geometries>");
  description.insert(end, "<WiringObject Name='ethernet_1' ComponentType='NetworkInOut' ConnectorType='RJ45' SignalType='Ethernet' PinCount='8'/>"
      "<WiringObject Name='wireless_1' ComponentType='NetworkInOut' ConnectorType='Wireless' SignalType='Ethernet' PinCount='0'/>"
      "<WiringObject Name='51000000-0000-4000-8000-000000000001' ComponentType='NetworkInOut' ConnectorType='RJ45' SignalType='Ethernet' PinCount='8'/>");
  std::string resourceError;
  assert(tests::archive::WriteStoredZipWithRawNames(fixture, {{"description.xml", description},
      {"models/gltf/body.glb", tests::gdtf::BuildMinimalValidGlb()}}, resourceError));
  std::ifstream file(fixture, std::ios::binary);
  const std::string gdtf(std::istreambuf_iterator<char>(file), {});
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, {{"GeneralSceneDescription.xml", xml}, {"fixture.gdtf", gdtf}}, error));
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
  bool publishedOnly = false;
  const auto inspect = [&](auto &&self, const tinyxml2::XMLElement &element) -> void {
    if (std::string(element.Name()) == "Address") {
      const auto key = perastage::patch::DmxInteger(element.Attribute("break") ? element.Attribute("break") : "0", true);
      publishedOnly |= (element.GetText() && std::string(element.GetText()).find('.') != std::string::npos) ||
          (key && key->front() == '-');
    }
    for (auto *child = element.FirstChildElement(); child; child = child->NextSiblingElement()) self(self, *child);
  };
  inspect(inspect, *doc.RootElement());
  assert(result.schema.status == (publishedOnly ? ValidationStatus::Invalid : ValidationStatus::Valid));
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
  const auto dir = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "perastage-address-test";
  fs::create_directories(dir);
  Schema(Source());
  Archive(dir / "source.mvr", Source());
  auto scene = Import(dir / "source.mvr");
  assert(scene.fixtures.at(single).address == "1.17");
  assert(scene.fixtures.at(multi).address == "3.1");
  assert(scene.fixtures.at(nonzero).address.empty());
  assert(scene.preservationIssues.empty());
  Export(scene, dir / "unchanged.mvr");
  tinyxml2::XMLDocument projection;
  assert(projection.Parse(Source().c_str()) == tinyxml2::XML_SUCCESS);
  assert(mvr::CheckFixtureAddressProjection(*projection.RootElement(), scene).empty());
  auto *fixtureXml = projection.RootElement()->FirstChildElement("Scene")->FirstChildElement("Layers")
      ->FirstChildElement("Layer")->FirstChildElement("ChildList")->FirstChildElement("Fixture")
      ->NextSiblingElement("Fixture");
  auto *assignments = fixtureXml->FirstChildElement("Addresses");
  assignments->DeleteChild(assignments->FirstChildElement("Address")->NextSiblingElement("Address"));
  assert(!mvr::CheckFixtureAddressProjection(*projection.RootElement(), scene).empty());
  auto &config = ConfigManager::Get();
  config.Reset(); config.GetScene() = scene;
  MvrExporter standalone;
  assert(standalone.ExportToFile((dir / "standalone.mvr").string()));
  for (int cycle = 1; cycle <= 3; ++cycle) {
    const auto stem = "cycle" + std::to_string(cycle);
    Export(config.GetScene(), dir / (stem + "-standalone.mvr"));
    assert(config.SaveProject((dir / (stem + ".pstg")).string()));
    config.Reset();
    assert(config.LoadProject((dir / (stem + ".pstg")).string()));
    Export(config.GetScene(), dir / (stem + ".mvr"));
  }
  config.Reset(); config.GetScene() = scene;
  config.PushUndoState("edit fixture primary assignment");
  assert(perastage::patch::SetFixturePrimaryPatch(config.GetScene().fixtures.at(multi), "5.17"));
  Export(config.GetScene(), dir / "edited.mvr");
  assert(config.SaveProject((dir / "edited.pstg").string()));
  config.Undo(); Export(config.GetScene(), dir / "undo.mvr");
  config.Redo(); Export(config.GetScene(), dir / "redo.mvr");
  config.Reset(); assert(config.LoadProject((dir / "edited.pstg").string()));
  Export(config.GetScene(), dir / "edited-reopened.mvr");

  auto cleared = scene;
  assert(perastage::patch::SetFixturePrimaryPatch(cleared.fixtures.at(multi), ""));
  Export(cleared, dir / "cleared.mvr");
  config.GetScene() = cleared;
  assert(config.SaveProject((dir / "cleared.pstg").string()));
  config.Reset(); assert(config.LoadProject((dir / "cleared.pstg").string()));
  Export(config.GetScene(), dir / "cleared-reopened.mvr");

  auto altered = scene;
  altered.fixtures.at(multi).instanceName = "Ordinary scene edit";
  scene_grouping::ObjectSelection selection;
  selection.fixtures = {single, multi};
  const auto grouped = scene_grouping::GroupSelection(altered, selection);
  assert(grouped.changed);
  assert(scene_grouping::GroupSelection(altered, selection).changed);
  Export(altered, dir / "nested-groups.mvr");
  scene_grouping::TranslateTargets(altered, {{MvrNodeType::GroupObject, grouped.groupUuid}}, {10, 20, 30});
  assert(scene_grouping::RemoveSelectionFromGroup(altered, selection).changed);
  layerdomain::AssignLayer(altered, altered.fixtures.at(multi), layerdomain::DefaultLayerUuid());
  Export(altered, dir / "hierarchy.mvr");

  auto merged = scene;
  const auto analysis = mvr::AnalyzeImportedSceneMerge(merged, scene);
  mvr::ApplyImportedSceneMergeAtomically(merged, scene, analysis);
  assert(merged.fixtures.at(analysis.uuidMap.at(multi)).dmxAddresses == scene.fixtures.at(multi).dmxAddresses);
  Export(merged, dir / "merged.mvr");
  auto copied = scene;
  SelectionState selected;
  selected.SetSelectedFixtures({multi});
  scene_clipboard::Service clipboard;
  assert(clipboard.Capture(copied, selected, 1));
  const auto paste = clipboard.Paste(copied, 1);
  assert(paste.changed);
  assert(copied.fixtures.at(paste.uuidRemap.at(multi)).dmxAddresses == scene.fixtures.at(multi).dmxAddresses);
  Export(copied, dir / "copied.mvr");

  auto fullyPatched = scene;
  // Whole-scene AutoPatch remains primary-only and keeps every interface.
  fullyPatched.fixtures.erase(nonzero);
  assert(AutoPatcher::AutoPatch(fullyPatched));
  Export(fullyPatched, dir / "auto-all.mvr");
  auto planned = scene;
  assert(AutoPatcher::AutoPatchSelection(planned, {multi}));
  Export(planned, dir / "auto-patched.mvr");
  const auto before = planned.fixtures.at(multi);
  const auto conflict = AutoPatcher::AutoPatchSelection(planned, {multi, nonzero});
  assert(!conflict && conflict.conflict == perastage::patch::FixturePatchConflict::MissingPrimary);
  assert(planned.fixtures.at(multi).address == before.address &&
         planned.fixtures.at(multi).dmxAddresses == before.dmxAddresses);
  assert(!perastage::patch::SetFixturePrimaryPatch(scene.fixtures.at(nonzero), "1.1"));

  config.Reset(); config.GetScene() = scene;
  config.SetSelectedFixtures({multi});
  assert(!config.CanUndo());
  assert(fixture_patch_workflow::AutoPatch(config));
  assert(config.CanUndo());
  Export(config.GetScene(), dir / "auto-gui.mvr");
  config.Undo();
  assert(config.GetScene().fixtures.at(multi).dmxAddresses == scene.fixtures.at(multi).dmxAddresses);
  config.Redo();
  assert(config.GetScene().fixtures.at(multi).dmxAddresses == planned.fixtures.at(multi).dmxAddresses);
  config.Reset(); config.GetScene() = scene;
  config.SetSelectedFixtures({multi, nonzero});
  assert(!fixture_patch_workflow::AutoPatch(config));
  assert(!config.CanUndo());
  assert(config.GetScene().fixtures.at(multi).dmxAddresses == scene.fixtures.at(multi).dmxAddresses);

  auto exhausted = scene;
  assert(perastage::patch::SetFixturePrimaryPatch(exhausted.fixtures.at(single), "2147483647.512"));
  const auto exhaustedBefore = exhausted.fixtures.at(multi);
  assert(!AutoPatcher::AutoPatchSelection(exhausted, {multi}));
  assert(exhausted.fixtures.at(multi).dmxAddresses == exhaustedBefore.dmxAddresses);
  assert(exhausted.fixtures.at(multi).address == exhaustedBefore.address);
  assert(!AutoPatcher::AutoPatch(exhausted, 2147483647, 512));
  assert(exhausted.fixtures.at(multi).dmxAddresses == exhaustedBefore.dmxAddresses);

  auto directEdit = scene;
  directEdit.fixtures.at(multi).address = "6.121";
  Export(directEdit, dir / "legacy-edited.mvr");
  assert(directEdit.fixtures.at(multi).dmxAddresses == scene.fixtures.at(multi).dmxAddresses);
  directEdit.fixtures.at(nonzero).address = "1.1";
  MvrExporter exporter;
  std::vector<uint8_t> bytes{1};
  assert(!exporter.ExportCanonicalSnapshotToBuffer(directEdit, bytes) && bytes.empty());

  for (const auto &[name, addresses] : std::vector<std::pair<std::string, std::string>>{
      {"empty", "<Addresses/>"}, {"absent", ""},
      {"spelling", "<Addresses><Address break='+0007'>+0002049</Address><Address>  +001025  </Address></Addresses>"},
      {"published", "<Addresses><Address break='0'>3.1</Address><Address break='-7'>9.512</Address></Addresses>"},
      {"large", "<Addresses><Address>999999999999999999</Address><Address break='999999999999999999'>17</Address></Addresses>"}}) {
    const auto xml = Source(addresses);
    Schema(xml);
    Archive(dir / (name + "-source.mvr"), xml);
    const auto imported = Import(dir / (name + "-source.mvr"));
    assert(imported.preservationIssues.empty());
    Export(imported, dir / (name + ".mvr"));
  }

  for (const auto *addresses : {
      "<Addresses><Address>1</Address><Address break='0'>2</Address></Addresses>",
      "<Addresses><Address break='7'>1</Address><Address break='+007'>2</Address></Addresses>",
      "<Addresses><Address/></Addresses>", "<Addresses><Address>0</Address></Addresses>",
      "<Addresses><Address break='bad'>1</Address></Addresses>",
      "<Addresses><Address>1.513</Address></Addresses>",
      "<Addresses><Address>1<Extra/></Address></Addresses>",
      "<Addresses><Address future='x'>1</Address></Addresses>",
      "<Addresses future='x'/>", "<Addresses/><Addresses/>",
      "<Addresses><Network ipv4='127.0.0.1'/></Addresses>",
      "<Addresses><Extension/></Addresses>"}) {
    Archive(dir / "invalid.mvr", Source(addresses));
    const auto invalid = Import(dir / "invalid.mvr");
    assert(!invalid.preservationIssues.empty());
    bytes = {1};
    assert(!exporter.ExportCanonicalSnapshotToBuffer(invalid, bytes) && bytes.empty());
    config.GetScene() = scene;
    const auto path = dir / "retained.pstg";
    assert(config.SaveProject(path.string()));
    const auto read = [&] { std::ifstream f(path, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); };
    const auto previous = read();
    config.GetScene() = invalid;
    assert(!config.SaveProject(path.string()));
    assert(config.GetLastProjectSaveFailure().stage == "SerializeScene");
    assert(read() == previous);
  }
  RunFixtureNetworkCases(dir, scene, {Source, Archive, Import, Export});
  RunSupportAddressCases(dir, {Source, Archive, Import, Export});
  config.Reset();
}
