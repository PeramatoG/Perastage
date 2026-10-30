/* This file is part of Perastage. */
#include "mvr_truss_address_cases.h"
#include "configmanager.h"
#include "fixture_network_reference.h"
#include "layer_identity.h"
#include "mvrexporter.h"
#include "mvrscenemerger.h"
#include "mvr_truss_address_preservation.h"
#include "scene_clipboard.h"
#include "scene_object_truss_converter.h"
#include "inspection/xml_schema_validation.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
namespace {
constexpr auto id = "51000000-0000-4000-8000-000000000001";
constexpr auto definition = "61000000-0000-4000-8000-000000000001";
std::string Source(const std::string &addresses, bool binding, bool symbol = false) {
  const std::string geometry = symbol ? "<Symbol uuid='71000000-0000-4000-8000-000000000001' symdef='" +
      std::string(definition) + "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,0}</Matrix></Symbol>" :
      "<Geometry3D fileName='truss.glb'><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,0}</Matrix></Geometry3D>";
  return "<GeneralSceneDescription verMajor='1' verMinor='6' provider='TrussTest' providerVersion='1'>"
      "<Scene><Layers><Layer uuid='11000000-0000-4000-8000-000000000001' name='Rig'><ChildList>"
      "<Truss uuid='" + std::string(id) + "' name='Rig.Left'><Matrix>{1,0,0}{0,1,0}{0,0,1}{1234.567890123,0,0}</Matrix>"
      "<Classing>81000000-0000-4000-8000-000000000001</Classing><Geometries>" + geometry + "</Geometries>"
      "<Function>Lighting</Function>" + (binding ? "<GDTFSpec>fixture.gdtf</GDTFSpec><GDTFMode>Mode</GDTFMode>" : "") + addresses +
      "<FixtureID>Truss.007</FixtureID><FixtureIDNumeric>17</FixtureIDNumeric><UnitNumber>0</UnitNumber>"
      "<CustomIdType>0</CustomIdType><CustomId>0</CustomId></Truss></ChildList></Layer></Layers>"
      "<AUXData><Class uuid='81000000-0000-4000-8000-000000000001' name='Rigging'/>" +
      (symbol ? "<Symdef uuid='" + std::string(definition) + "' name='TrussType'><ChildList><Geometry3D fileName='truss.glb'/></ChildList></Symdef>" : "") +
      "</AUXData></Scene></GeneralSceneDescription>";
}
std::string Bytes(const fs::path &path) {
  std::ifstream file(path, std::ios::binary); return {std::istreambuf_iterator<char>(file), {}};
}
void Archive(const fs::path &path, const std::string &xml, const FixtureNetworkTestServices &services) {
  services.archive(path, xml);
  std::string gdtf, error;
  assert(tests::zip::ReadEntry(path, "fixture.gdtf", &gdtf));
  assert(tests::archive::WriteStoredZipWithRawNames(path, {{"GeneralSceneDescription.xml", xml},
      {"fixture.gdtf", gdtf}, {"truss.glb", tests::gdtf::BuildMinimalValidGlb()}}, error));
}
void SchemaEvidence(const std::string &xml, bool output) {
  using namespace perastage::inspection;
  tinyxml2::XMLDocument doc; assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  // The pinned IPv6 pattern also rejects the fully expanded lowercase test value.
  const bool ipv6 = xml.find("ipv6=") != std::string::npos;
  const auto full = ValidateXmlAgainstSchema(xml, Mvr16Schema());
  assert(full.xml.status == ValidationStatus::Valid);
  // Full exported XML has the documented official UserData/XSD discrepancy.
  if (full.schema.status != (output || ipv6 ? ValidationStatus::Invalid : ValidationStatus::Valid)) {
    std::cerr << "Unexpected full XSD result: output=" << output << " ipv6=" << ipv6 << "\n";
    for (const auto &diagnostic : full.schema.diagnostics) std::cerr << diagnostic.message << "\n";
  }
  assert(full.schema.status == (output || ipv6 ? ValidationStatus::Invalid : ValidationStatus::Valid));
  if (auto *data = doc.RootElement()->FirstChildElement("UserData")) doc.RootElement()->DeleteChild(data);
  tinyxml2::XMLPrinter printer; doc.Print(&printer);
  const auto standard = ValidateXmlAgainstSchema(printer.CStr(), Mvr16Schema());
  assert(standard.schema.status == (ipv6 ? ValidationStatus::Invalid : ValidationStatus::Valid));
}
void Export(const MvrScene &scene, const fs::path &path) {
  MvrExporter exporter; assert(exporter.ExportCanonicalSnapshotToFile(scene, path.string()));
  std::string xml; assert(tests::zip::ReadEntry(path, "GeneralSceneDescription.xml", &xml));
  SchemaEvidence(xml, true);
}
}
void RunTrussAddressCases(const fs::path &dir, const FixtureNetworkTestServices &services) {
  auto &config = ConfigManager::Get();
  const std::string mixed = "<Addresses><Address>00017</Address><Address break='+007'>2049</Address>"
      "<Network geometry='ethernet_1' ipv4='192.168.001.005' subnetmask='255.255.0.0'/>"
      "<Network geometry='wireless_1' ipv6='2001:0db8:0000:0000:0000:0000:0000:0001' dhcp='false' hostname=''/>"
      "<Network geometry='51000000-0000-4000-8000-000000000001' dhcp='on' hostname='Rig.Left'/></Addresses>";
  for (const auto &[name, assignments] : std::vector<std::pair<std::string, std::string>>{
      {"none", ""}, {"bound-none", ""}, {"bound-dmx", "<Addresses><Address break='9'>513</Address></Addresses>"}, {"empty", "<Addresses/>"}, {"single", "<Addresses><Address>17</Address></Addresses>"},
      {"multi", "<Addresses><Address break='0'>17</Address><Address break='19'>4097</Address></Addresses>"},
      {"nonzero", "<Addresses><Address break='7'>2049</Address></Addresses>"},
      {"only", "<Addresses><Network geometry='ethernet_1'/></Addresses>"}, {"mixed", mixed}, {"symbol", mixed},
      {"ipv6", "<Addresses><Network geometry='wireless_1' ipv6='2001:DB8::A'/></Addresses>"}}) {
    const bool binding = name.starts_with("bound") || name == "only" || name == "mixed" || name == "symbol" || name == "ipv6";
    const auto stem = "truss-" + name;
    const auto source = Source(assignments, binding, name == "symbol"); SchemaEvidence(source, false);
    Archive(dir / (stem + "-source.mvr"), source, services);
    auto scene = services.import(dir / (stem + "-source.mvr"));
    for (const auto &issue : scene.preservationIssues)
      std::cerr << "Truss case " << name << ": " << issue.sourceLocation << ": " << issue.reason << "\n";
    assert(scene.preservationIssues.empty());
    const auto original = scene.trusses.at(id);
    assert(original.identifierSource && original.identifierSource->customIdPresent);
    Export(scene, dir / (stem + ".mvr"));
    config.Reset(); config.GetScene() = scene;
    for (int cycle = 1; cycle <= 3; ++cycle) {
      const auto file = stem + "-cycle" + std::to_string(cycle);
      Export(config.GetScene(), dir / (file + "-standalone.mvr"));
      assert(config.SaveProject((dir / (file + ".pstg")).string()));
      config.Reset(); assert(config.LoadProject((dir / (file + ".pstg")).string()));
      assert(config.GetScene().trusses.at(id).dmxAddresses == original.dmxAddresses);
      Export(config.GetScene(), dir / (file + ".mvr"));
    }
    if (name != "mixed") continue;
    config.GetScene() = scene; config.PushUndoState("edit Truss metadata");
    auto &edited = config.GetScene().trusses.at(id);
    edited.name = "Edited";
    edited.function = "Video"; edited.hasManualLoadOverride = true; edited.manualLoadKg = 100.0f;
    auto dimensionEdit = scene;
    dimensionEdit.trusses.at(id).lengthMm += 1000.0f;
    MvrExporter refusedDimensions; std::vector<unsigned char> refusedBytes;
    assert(!refusedDimensions.ExportCanonicalSnapshotToBuffer(dimensionEdit, refusedBytes));
    assert(dimensionEdit.trusses.at(id).dmxAddresses == original.dmxAddresses);
    Export(config.GetScene(), dir / "truss-edited.mvr");
    config.Undo(); Export(config.GetScene(), dir / "truss-undo.mvr");
    config.Redo(); Export(config.GetScene(), dir / "truss-redo.mvr");
    const auto editedProject = dir / "truss-edited.pstg";
    assert(config.SaveProject(editedProject.string()));
    config.Reset(); assert(config.LoadProject(editedProject.string()));
    const auto &restoredEdit = config.GetScene().trusses.at(id);
    assert(restoredEdit.name == "Edited" && restoredEdit.function == "Video");
    assert(restoredEdit.hasManualLoadOverride && restoredEdit.manualLoadKg == 100.0f);
    assert(restoredEdit.dmxAddresses == original.dmxAddresses);
    Export(config.GetScene(), dir / "truss-edit-reopened.mvr");
    auto merged = scene;
    const auto plan = mvr::AnalyzeImportedSceneMerge(merged, scene);
    mvr::ApplyImportedSceneMergeAtomically(merged, scene, plan);
    assert(merged.trusses.at(plan.uuidMap.at(id)).dmxAddresses == original.dmxAddresses);
    Export(merged, dir / "truss-merged.mvr");
    SelectionState selected; selected.SetSelectedTrusses({id}); scene_clipboard::Service clipboard;
    auto copied = scene; assert(clipboard.Capture(copied, selected, 1)); assert(clipboard.Paste(copied, 1).changed);
    Export(copied, dir / "truss-copied.mvr");
    Fixture anchor; anchor.uuid = "91000000-0000-4000-8000-000000000001";
    anchor.gdtfSpec = original.gdtfSpec; anchor.gdtfMode = "Mode";
    layerdomain::AssignLayer(scene, anchor, original.layerUuid); scene.fixtures[anchor.uuid] = anchor;
    scene_grouping::ObjectSelection selection; selection.fixtures = {anchor.uuid}; selection.trusses = {id};
    assert(scene_grouping::GroupSelection(scene, selection).changed);
    Export(scene, dir / "truss-grouped.mvr");
    assert(scene_grouping::RemoveSelectionFromGroup(scene, selection).changed);
    layerdomain::AssignLayer(scene, scene.trusses.at(id), layerdomain::DefaultLayerUuid());
    Export(scene, dir / "truss-layer.mvr");
    auto auxiliaryScene = scene;
    const auto auxiliaryPath = dir / "truss-auxiliary.gdtf";
    tests::gdtf::BuildMinimalValidFixture().WithDmxMode("AuxiliaryMode", "Root").WriteArchive(auxiliaryPath);
    auxiliaryScene.trusses.at(id).perastageAuxGdtfArchivePath = auxiliaryPath.string();
    Export(auxiliaryScene, dir / "truss-auxiliary.mvr");
    std::string projectedXml;
    assert(tests::zip::ReadEntry(dir / "truss-mixed.mvr", "GeneralSceneDescription.xml", &projectedXml));
    tinyxml2::XMLDocument projection; assert(projection.Parse(projectedXml.c_str()) == tinyxml2::XML_SUCCESS);
    assert(!mvr::CheckSerializedTrussAddressBindings(*projection.RootElement(), scene,
        [&](const std::string &) { return auxiliaryPath; }).empty());
    auto candidate = original;
    assert(fixture_network::RebindNode(candidate, "missing.gdtf", dir / "missing.gdtf").conflict == fixture_network::Conflict::UnavailableGdtf);
    assert(candidate.gdtfSpec == original.gdtfSpec && candidate.dmxAddresses == original.dmxAddresses);
    assert(fixture_network::RebindNode(candidate, auxiliaryPath.string(), auxiliaryPath).conflict == fixture_network::Conflict::InvalidBinding);
    assert(candidate.gdtfSpec == original.gdtfSpec && candidate.dmxAddresses == original.dmxAddresses);
    config.GetScene() = scene; const auto retained = dir / "truss-retained.pstg";
    assert(config.SaveProject(retained.string())); const auto previous = Bytes(retained);
    for (int failure = 0; failure < 10; ++failure) {
      auto invalid = scene; auto &truss = invalid.trusses.at(id);
      if (failure == 0) truss.gdtfSpec = "missing.gdtf";
      if (failure == 1) truss.gdtfMode.clear();
      if (failure == 2) { truss.perastageAuxGdtfArchivePath = truss.gdtfSpec; truss.gdtfSpec.clear(); }
      if (failure == 3) truss.dmxAddresses->networks.front().geometry = "Mismatched";
      if (failure == 8) truss.weightKg += 1.0f;
      if (failure == 9) truss.manufacturer = "Edited type manufacturer";
      if (failure == 7) truss.gdtfMode = "MissingMode";
      if (failure == 5) truss.perastageAuxGdtfArchivePath = "missing-auxiliary.gdtf";
      if (failure == 6) truss.lengthMm += 1000.0f;
      if (failure == 4) {
        const auto duplicate = dir / "truss-ambiguous.gdtf"; std::string error;
        auto ambiguous = tests::gdtf::BuildMinimalValidFixture().WithDmxMode("Mode", "Root").BuildDescriptionXml();
        ambiguous.insert(ambiguous.find("</Geometries>"),
            "<WiringObject Name='ethernet_1' ComponentType='NetworkInOut' ConnectorType='RJ45' SignalType='Ethernet' PinCount='8'/>"
            "<WiringObject Name='ethernet_1' ComponentType='NetworkInOut' ConnectorType='RJ45' SignalType='Ethernet' PinCount='8'/>");
        assert(tests::archive::WriteStoredZipWithRawNames(duplicate, {{"description.xml", ambiguous}}, error));
        truss.gdtfSpec = duplicate.string();
      }
      if (failure == 3 || failure == 4) {
        auto path = fs::path(truss.gdtfSpec); if (path.is_relative()) path = fs::path(scene.basePath) / path;
        assert(fixture_network::InspectTrussReferences(truss, path).conflict ==
            (failure == 3 ? fixture_network::Conflict::UnknownGeometry : fixture_network::Conflict::AmbiguousGeometry));
      }
      config.GetScene() = invalid; assert(!config.SaveProject(retained.string())); assert(Bytes(retained) == previous);
      assert(config.GetScene().trusses.at(id).dmxAddresses == truss.dmxAddresses);
    }
  }
  for (const auto *assignments : {"<Addresses/><Addresses/>", "<Addresses future='x'/>",
      "<Addresses><Address future='x'>17</Address></Addresses>",
      "<Addresses><Address break='7'>1</Address><Address break='+007'>2</Address></Addresses>",
      "<Addresses><Network/></Addresses>", "<Addresses><Network geometry='ethernet_1' future='x'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1'/><Address>17</Address></Addresses>",
      "<Addresses><Network geometry='ethernet_1'><Child/></Network></Addresses>"}) {
    Archive(dir / "truss-invalid.mvr", Source(assignments, true), services);
    const auto invalid = services.import(dir / "truss-invalid.mvr"); assert(!invalid.preservationIssues.empty());
    MvrExporter exporter; std::vector<unsigned char> bytes; assert(!exporter.ExportCanonicalSnapshotToBuffer(invalid, bytes));
  }
  auto unavailableSource = Source(mixed, true);
  unavailableSource.replace(unavailableSource.find("fixture.gdtf"), std::string("fixture.gdtf").size(), "unavailable.gdtf");
  Archive(dir / "truss-unavailable.mvr", unavailableSource, services);
  const auto unavailable = services.import(dir / "truss-unavailable.mvr");
  assert(unavailable.trusses.at(id).dmxAddresses->networks.size() == 3);
  const auto diagnostics = mvr::CheckTrussAddressBindings(unavailable);
  assert(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto &diagnostic) {
    return diagnostic.code == MvrExportDiagnosticCode::ResourceMissing;
  }));
  config.Reset();
}
