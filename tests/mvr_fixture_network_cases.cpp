/* This file is part of Perastage. */
#include "mvr_fixture_network_cases.h"
#include "fixture_network_reference.h"
#include "fixture_patch_service.h"
#include "autopatcher.h"
#include "configmanager.h"
#include "mvrexporter.h"
#include "mvr_standard_preservation.h"
#include "mvrscenemerger.h"
#include "inspection/xml_schema_validation.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include "gdtf/editor/project_fixture_gdtf_apply_adapter.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include <cassert>
#include <fstream>

namespace fs = std::filesystem;
namespace {
constexpr auto multi = "51000000-0000-4000-8000-000000000002";
void Schema(const std::string &xml, bool ipv6, bool exported = false) {
  using namespace perastage::inspection;
  tinyxml2::XMLDocument document;
  assert(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  if (exported) document.RootElement()->DeleteChild(document.RootElement()->FirstChildElement("UserData"));
  tinyxml2::XMLPrinter printer;
  document.Print(&printer);
  const auto validation = ValidateXmlAgainstSchema(printer.CStr(), Mvr16Schema());
  assert(validation.xml.status == ValidationStatus::Valid);
  // The pinned XSD's anchored IPv6 regex disagrees with normative notation.
  assert(validation.schema.status == (ipv6 ? ValidationStatus::Invalid : ValidationStatus::Valid));
}
std::string Bytes(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}
}
void RunFixtureNetworkCases(const fs::path &dir, const MvrScene &scene,
                           const FixtureNetworkTestServices &services) {
  auto &config = ConfigManager::Get();
  for (const auto &[name, content] : std::vector<std::pair<std::string, std::string>>{
      {"none", ""}, {"empty", "<Addresses/>"},
      {"only", "<Addresses><Network geometry='ethernet_1'/></Addresses>"},
      {"repeat", "<Addresses><Network geometry='ethernet_1' hostname='First'/><Network geometry='ethernet_1' hostname='Second'/></Addresses>"},
      {"ipv4", "<Addresses><Network geometry='ethernet_1' ipv4='192.168.001.005' subnetmask='255.255.0.0'/></Addresses>"},
      {"ipv6", "<Addresses><Network geometry='wireless_1' ipv6='2001:0db8:85a3:0000:0000:8a2e:0370:7344'/></Addresses>"},
      {"dhcp", "<Addresses><Network geometry='ethernet_1' dhcp='true' hostname=''/><Network geometry='wireless_1' dhcp='false' hostname='Rig.Right'/></Addresses>"},
      {"lexical", "<Addresses><Network geometry='wireless_1' ipv6='2001:DB8::A'/><Network geometry='ethernet_1' dhcp='on'/></Addresses>"}}) {
    const auto stem = "network-" + name;
    Schema(services.source(content), name == "ipv6" || name == "lexical");
    services.archive(dir / (stem + "-source.mvr"), services.source(content));
    auto imported = services.import(dir / (stem + "-source.mvr"));
    assert(imported.preservationIssues.empty());
    // IPv6 lexical forms are normative; the unchanged pinned XSD can reject them.
    MvrExporter exporter;
    assert(exporter.ExportCanonicalSnapshotToFile(imported, (dir / (stem + ".mvr")).string()));
    std::string exportedXml;
    assert(tests::zip::ReadEntry(dir / (stem + ".mvr"), "GeneralSceneDescription.xml", &exportedXml));
    Schema(exportedXml, name == "ipv6" || name == "lexical", true);
    config.GetScene() = imported;
    assert(config.SaveProject((dir / (stem + ".pstg")).string()));
    config.Reset(); assert(config.LoadProject((dir / (stem + ".pstg")).string()));
    assert(config.GetScene().fixtures.at(multi).dmxAddresses == imported.fixtures.at(multi).dmxAddresses);
    if (name == "only") {
      auto autoOnly = imported;
      assert(AutoPatcher::AutoPatchSelection(autoOnly, {multi}));
      services.exportScene(autoOnly, dir / "network-auto-only.mvr");
      assert(perastage::patch::SetFixturePrimaryPatch(imported.fixtures.at(multi), "2.17"));
      services.exportScene(imported, dir / "network-added-primary.mvr");
      assert(perastage::patch::SetFixturePrimaryPatch(imported.fixtures.at(multi), ""));
      services.exportScene(imported, dir / "network-removed-primary.mvr");
    }
  }
  // Resolve/rebind validates names without applying MVR UUID remapping or guesses.
  auto fixture = scene.fixtures.at(multi);
  auto path = fs::path(fixture.gdtfSpec);
  if (!path.is_absolute()) path = fs::path(scene.basePath) / path;
  assert(fixture_network::InspectReferences(fixture, path));
  const auto previous = fixture;
  assert(fixture_network::Rebind(fixture, "", path).conflict == fixture_network::Conflict::InvalidBinding);
  assert(fixture.gdtfSpec == previous.gdtfSpec);
  const auto unavailable = fixture_network::Rebind(fixture, "missing.gdtf", dir / "missing.gdtf");
  assert(!unavailable && unavailable.conflict == fixture_network::Conflict::UnavailableGdtf);
  assert(fixture.gdtfSpec == previous.gdtfSpec && fixture.dmxAddresses == previous.dmxAddresses);
  const auto replacement = dir / "replacement.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(replacement);
  const auto missing = fixture_network::Rebind(fixture, replacement.string(), replacement);
  assert(!missing && missing.conflict == fixture_network::Conflict::UnknownGeometry);
  assert(fixture.gdtfSpec == previous.gdtfSpec);
  assert(fixture_network::Rebind(fixture, path.string(), path));
  assert(fixture.dmxAddresses == previous.dmxAddresses);
  auto rebound = scene;
  rebound.fixtures.at(multi) = fixture;
  services.exportScene(rebound, dir / "network-rebound.mvr");
  auto recoveredOriginal = scene;
  recoveredOriginal.fixtures.at(multi).gdtfSpec.clear();
  services.exportScene(recoveredOriginal, dir / "network-original-reference.mvr");
  auto changed = scene;
  changed.fixtures.at(multi).gdtfSpec = replacement.string();
  assert(!mvr::CheckStandardPreservation(changed).empty());
  auto current = scene;
  for (auto &[uuid, item] : current.fixtures) {
    item.gdtfSpec = replacement.string();
    item.dmxAddresses->networks.clear();
  }
  auto mergeAnalysis = mvr::AnalyzeImportedSceneMerge(current, scene);
  for (const auto &[type, identity] : mergeAnalysis.currentFixtureTypes)
    mergeAnalysis.fixtureTypeDecisions[type] = mvr::MvrMergeFixtureTypeDecision::UseCurrentDefinition;
  const auto mergeResult = mvr::ApplyImportedSceneMergeAtomically(current, scene, mergeAnalysis);
  assert(!mergeResult.networkConflicts.empty() && mergeResult.fixtureTypeConflictsBlocked);
  assert(current.fixtures.size() == scene.fixtures.size());
  assert(current.fixtures.at(multi).gdtfSpec == replacement.string());
  std::unordered_map<std::string, Fixture> fixtures{{multi, previous}};
  gdtf::GdtfApplyRequest request;
  request.contextKind = gdtf::GdtfEditorContextKind::ProjectFixture;
  request.stableHostId = multi;
  request.sourcePath = replacement;
  request.changedContextFields.insert(gdtf::GdtfFieldId::SourceFileReference);
  request.values.sourceFileReference = replacement.string();
  gdtf::ProjectFixtureGdtfApplyServices applyServices;
  applyServices.inspectNetworks = fixture_network::InspectReferences;
  const auto rejected = gdtf::ProjectFixtureGdtfApplyAdapter(applyServices).Apply({request, &fixtures});
  assert(!rejected.common.success && rejected.networkConflict);
  assert(rejected.updatedFixtures.empty() && fixtures.at(multi).gdtfSpec == previous.gdtfSpec);

  const auto retained = dir / "network-retained.pstg";
  config.GetScene() = scene;
  assert(config.SaveProject(retained.string()));
  const auto originalBytes = Bytes(retained);
  for (const auto *content : {
      "<Addresses><Network/></Addresses>",
      "<Addresses><Network geometry=' '/></Addresses>",
      "<Addresses><Network geometry='ethernet_1' future='x'/></Addresses>",
      "<Addresses><Network Interface='ethernet_1' IPv4='127.0.0.1'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1' ipv4='999.1.2.3'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1' subnetmask='255.x.0.0'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1' ipv6='1::2::3'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1' dhcp='maybe'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1' hostname='a&#10;b'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1' hostname='a&#9;b'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1' hostname='a&#13;b'/></Addresses>",
      "<Addresses><Network geometry='ethernet&#9;1'/></Addresses>",
      "<Addresses><Network geometry='ethernet_1'><Extra/></Network></Addresses>",
      "<Addresses><Network geometry='ethernet_1'>text</Network></Addresses>",
      "<Addresses><Network geometry='ethernet_1'/><Address>1</Address></Addresses>",
      "<Addresses><Network geometry='ethernet_1'/></Addresses><Addresses/>",
      "<Addresses><Network geometry='ethernet_1'/><Extension/></Addresses>",
      "<Addresses><Network geometry='Ethernet_1'/></Addresses>"}) {
    services.archive(dir / "network-invalid.mvr", services.source(content));
    const auto invalid = services.import(dir / "network-invalid.mvr");
    assert(!mvr::CheckStandardPreservation(invalid).empty());
    config.GetScene() = invalid;
    assert(!config.SaveProject(retained.string()));
    assert(config.GetLastProjectSaveFailure().stage == "SerializeScene");
    assert(Bytes(retained) == originalBytes);
  }
  // Missing local resources retain assignments, with unverified reference evidence.
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(dir / "network-missing.mvr",
      {{"GeneralSceneDescription.xml", services.source("<Addresses><Network geometry='ethernet_1' hostname='Absent.Resource'/></Addresses>")}}, error));
  const auto unresolved = services.import(dir / "network-missing.mvr");
  assert(unresolved.fixtures.at(multi).dmxAddresses->networks.front().hostname == "Absent.Resource");
  assert(fixture_network::InspectReferences(unresolved.fixtures.at(multi), dir / "missing.gdtf").conflict ==
      fixture_network::Conflict::UnavailableGdtf);
  config.GetScene() = unresolved;
  assert(!config.SaveProject(retained.string()));
  assert(Bytes(retained) == originalBytes);
  auto duplicateDescription = tests::gdtf::BuildMinimalValidFixture().BuildDescriptionXml();
  duplicateDescription.insert(duplicateDescription.find("</Geometries>"),
      "<WiringObject Name='ethernet_1'/><Geometry Name='ethernet_1'/>");
  const auto duplicatePath = dir / "duplicate-geometry.gdtf";
  assert(tests::archive::WriteStoredZipWithRawNames(duplicatePath,
      {{"description.xml", duplicateDescription}}, error));
  const auto ambiguous = fixture_network::InspectReferences(previous, duplicatePath);
  assert(ambiguous.conflict == fixture_network::Conflict::AmbiguousGeometry);
  config.Reset();
}
