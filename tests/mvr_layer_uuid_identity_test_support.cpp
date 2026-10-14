#include "mvr_layer_uuid_identity_test_support.h"

#include "configmanager.h"
#include "gdtf_test_fixture_builder.h"
#include "primitive_model_resources.h"
#include "inspection/xml_schema_validation.h"
#include "layer_identity.h"
#include "layer_service.h"
#include "mvr_export_preparation.h"
#include "mvr_identity_recovery.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "support/zip_test_utils.h"
#include "uuidutils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <string>
#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {
namespace fs = std::filesystem;
constexpr const char *kLayerA = "11111111-1111-4111-8111-111111111111";
constexpr const char *kLayerB = "22222222-2222-4222-8222-222222222222";
constexpr const char *kLayerC = "33333333-3333-4333-8333-333333333333";
constexpr const char *kLayerD = "44444444-4444-4444-8444-444444444444";
constexpr const char *kObjectA = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1";
constexpr const char *kObjectB = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa2";
constexpr const char *kObjectC = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa3";
constexpr const char *kObjectD = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa4";

void AssertNoVisibleIdentityRepair(const MvrExporter &exporter) {
  for (const auto &diagnostic : exporter.GetExportDiagnostics())
    if (diagnostic.impact == MvrExportDiagnosticImpact::IdentityChanged)
      assert(!diagnostic.userVisible);
}

std::string ExportAndValidate(const MvrScene &scene, const fs::path &path) {
  MvrExporter exporter;
  assert(exporter.ExportCanonicalSnapshotToFile(scene, path.generic_string()));
  AssertNoVisibleIdentityRepair(exporter);
  std::string xml;
  assert(tests::zip::ReadEntry(path, "GeneralSceneDescription.xml", &xml));
  using namespace perastage::inspection;
  const auto validation = ValidateXmlAgainstSchema(xml, Mvr16Schema());
  assert(validation.xml.status == ValidationStatus::Valid);
  assert(validation.schema.status == ValidationStatus::Valid);
  return xml;
}

MvrScene Import(const fs::path &path) {
  MvrScene scene;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  assert(MvrImporter().ImportSceneFromFile(path.generic_string(), scene, options));
  return scene;
}

// Exercise the production reader with two empty and two equal authored names.
void TestAuthoredLayers(const fs::path &directory) {
  const auto input = directory / "authored.mvr";
  const std::string xml =
      std::string(R"(<GeneralSceneDescription verMajor="1" verMinor="6"><Scene><Layers>)") +
      "<Layer uuid=\"" + kLayerA + "\" name=\"\"><ChildList><GroupObject uuid=\"" +
      kObjectA + "\" name=\"A\"><ChildList/></GroupObject></ChildList></Layer>" +
      "<Layer uuid=\"" + kLayerB + "\" name=\"\"><ChildList><GroupObject uuid=\"" +
      kObjectB + "\" name=\"B\"><ChildList/></GroupObject></ChildList></Layer>" +
      "<Layer uuid=\"" + kLayerC + "\" name=\"LX\"><ChildList><GroupObject uuid=\"" +
      kObjectC + "\" name=\"C\"><ChildList/></GroupObject></ChildList></Layer>" +
      "<Layer uuid=\"" + kLayerD + "\" name=\"LX\"><ChildList><GroupObject uuid=\"" +
      kObjectD + "\" name=\"D\"><ChildList/></GroupObject></ChildList></Layer>" +
      "</Layers></Scene></GeneralSceneDescription>";
  using namespace perastage::inspection;
  assert(ValidateXmlAgainstSchema(xml, Mvr16Schema()).schema.status ==
         ValidationStatus::Valid);
  {
    wxFFileOutputStream file(input.string());
    wxZipOutputStream zip(file);
    assert(zip.PutNextEntry("GeneralSceneDescription.xml"));
    zip.Write(xml.data(), xml.size());
    assert(zip.Close());
  }
  auto scene = Import(input);
  auto check = [&](const MvrScene &value) {
    assert(value.layers.at(kLayerA).name.empty());
    assert(value.layers.at(kLayerB).name.empty());
    assert(value.layers.at(kLayerC).name == "LX");
    assert(value.layers.at(kLayerD).name == "LX");
    assert(value.groupObjects.at(kObjectA).layerUuid == kLayerA);
    assert(value.groupObjects.at(kObjectB).layerUuid == kLayerB);
    assert(value.groupObjects.at(kObjectC).layerUuid == kLayerC);
    assert(value.groupObjects.at(kObjectD).layerUuid == kLayerD);
    assert(layerdomain::PresentationName(value, kLayerA) !=
           layerdomain::PresentationName(value, kLayerB));
    assert(layerdomain::PresentationName(value, kLayerC) !=
           layerdomain::PresentationName(value, kLayerD));
  };
  check(scene);
  const auto export1 = directory / "first.mvr";
  ExportAndValidate(scene, export1);
  const auto roundtrip = Import(export1);
  check(roundtrip);
  const auto export2 = directory / "second.mvr";
  ExportAndValidate(roundtrip, export2);
  check(Import(export2));
  MvrScene repaired = roundtrip;
  assert(mvridentity::RecoverSceneIdentities(repaired, "second-roundtrip")
             .diagnostics.empty());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  cfg.GetScene() = scene;
  cfg.SetCurrentLayer(kLayerB);
  cfg.SetHiddenLayers({kLayerA});
  const auto project = directory / "layers.pstg";
  assert(cfg.SaveProject(project.string()));
  cfg.Reset();
  assert(cfg.LoadProject(project.string()));
  check(cfg.GetScene());
  assert(cfg.GetCurrentLayer() == kLayerB);
  assert(cfg.GetHiddenLayers().contains(kLayerA));
  assert(!cfg.GetHiddenLayers().contains(kLayerB));
}

// Verify all object types have UUID membership through rename, Undo and export preparation.
void TestDomainOperations(const fs::path &directory) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  auto &scene = cfg.GetScene();
  assert(scene.layers.contains(layerdomain::DefaultLayerUuid()));
  assert(CanonicalizeUuid(layerdomain::DefaultLayerUuid()) == layerdomain::DefaultLayerUuid());
  assert(!scene.layers.contains("layer_default"));
  auto prepared = mvr_export_preparation::Prepare(scene, CanonicalMvrExportOptions());
  assert(prepared.success);
  assert(prepared.diagnostics.empty());
  scene.layers[kLayerA] = Layer{kLayerA, "LX 1", {}, {}};
  scene.layers[kLayerB] = Layer{kLayerB, "LX 1", {}, {}};
  scene.basePath = directory.generic_string();
  const auto profile = directory / "layer-fixture.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(profile);
  const auto geometry = directory / "layer-truss.glb";
  assert(mvr::WritePrimitiveModelForToken("primitive:cube", geometry.string()));
  Fixture fixture;
  fixture.gdtfSpec = profile.string();
  fixture.gdtfMode = "Default";
  fixture.fixtureId = 1;
  fixture.unitNumber = 1;
  fixture.uuid = DeriveDeterministicUuid("test:layer:fixture");
  Truss truss;
  truss.symbolFile = geometry.string();
  truss.lengthMm = 1000.0f;
  truss.widthMm = 300.0f;
  truss.heightMm = 300.0f;
  truss.uuid = DeriveDeterministicUuid("test:layer:truss");
  Support support;
  support.uuid = DeriveDeterministicUuid("test:layer:support");
  SceneObject object;
  object.modelFile = "primitive:sphere";
  object.uuid = DeriveDeterministicUuid("test:layer:object");
  GroupObject group;
  group.uuid = kObjectA;
  layerdomain::AssignLayer(scene, fixture, kLayerA);
  layerdomain::AssignLayer(scene, truss, kLayerA);
  layerdomain::AssignLayer(scene, support, kLayerA);
  layerdomain::AssignLayer(scene, object, kLayerA);
  layerdomain::AssignLayer(scene, group, kLayerA);
  scene.fixtures[fixture.uuid] = fixture;
  scene.trusses[truss.uuid] = truss;
  scene.supports[support.uuid] = support;
  scene.sceneObjects[object.uuid] = object;
  scene.groupObjects[group.uuid] = group;
  cfg.SetCurrentLayer(kLayerA);
  cfg.SetHiddenLayers({kLayerA});
  assert(layerdomain::RenameLayer(cfg, kLayerA, "Front").status == layerdomain::LayerStatus::Success);
  auto check = [&](const MvrScene &value) {
    assert(value.fixtures.at(fixture.uuid).layerUuid == kLayerA);
    assert(value.trusses.at(truss.uuid).layerUuid == kLayerA);
    assert(value.supports.at(support.uuid).layerUuid == kLayerA);
    assert(value.sceneObjects.at(object.uuid).layerUuid == kLayerA);
    assert(value.groupObjects.at(group.uuid).layerUuid == kLayerA);
  };
  check(scene);
  assert(cfg.GetCurrentLayer() == kLayerA);
  assert(!cfg.IsLayerVisible(kLayerA));
  assert(cfg.IsLayerVisible(kLayerB));
  cfg.Undo();
  check(scene);
  assert(scene.layers.at(kLayerA).name == "LX 1");
  cfg.Redo();
  check(scene);
  assert(scene.layers.at(kLayerA).name == "Front");
  prepared = mvr_export_preparation::Prepare(scene, CanonicalMvrExportOptions());
  assert(prepared.success);
  assert(prepared.layerUuids.at(kLayerA) == kLayerA);
  check(prepared.scene);
  assert(mvridentity::RecoverSceneIdentities(prepared.scene, "again").diagnostics.empty());
  const auto allTypes = directory / "all-types.mvr";
  MvrExporter allTypesExporter;
  assert(allTypesExporter.ExportCanonicalSnapshotToFile(scene, allTypes.string()));
  AssertNoVisibleIdentityRepair(allTypesExporter);
  check(Import(allTypes));
  // The extension-free layer/group representation also validates against XSD.
  MvrScene layerScene;
  layerScene.layers = scene.layers;
  layerScene.groupObjects = scene.groupObjects;
  ExportAndValidate(layerScene, directory / "renamed.mvr");

  // UUID deletion must leave a same-named foreign layer and its children intact.
  GroupObject other;
  other.uuid = kObjectB;
  layerdomain::AssignLayer(scene, other, kLayerB);
  scene.groupObjects[other.uuid] = other;
  assert(layerdomain::DeleteLayer(cfg, kLayerA).status == layerdomain::LayerStatus::Success);
  assert(scene.fixtures.empty() && scene.trusses.empty() && scene.supports.empty());
  assert(scene.sceneObjects.empty());
  assert(scene.groupObjects.contains(kObjectB));
  cfg.Undo();
  check(scene);
}

void TestLegacyAndDefensiveRecovery() {
  MvrScene scene;
  scene.layers["layer_default"] = Layer{"layer_default", "No Layer", {}, {kObjectA}};
  GroupObject group;
  group.uuid = kObjectA;
  group.layer = "No Layer";
  scene.groupObjects[group.uuid] = group;
  layerdomain::MigrateLayerMembership(scene);
  assert(!scene.layers.contains("layer_default"));
  assert(scene.groupObjects.at(kObjectA).layerUuid == layerdomain::DefaultLayerUuid());
  layerdomain::MigrateLayerMembership(scene);
  assert(mvridentity::RecoverSceneIdentities(scene, "legacy").diagnostics.empty());
  const auto prepared = mvr_export_preparation::Prepare(scene, CanonicalMvrExportOptions());
  assert(prepared.success && prepared.diagnostics.empty());
  // Damage introduced after migration must still generate visible diagnostics.
  auto broken = scene;
  broken.layers["bad"] = Layer{"bad", "Damaged", {}, {}};
  const auto damage = mvr_export_preparation::Prepare(broken, CanonicalMvrExportOptions());
  assert(std::any_of(damage.diagnostics.begin(), damage.diagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.userVisible; }));
  assert(broken.layers.contains("bad")); // Snapshot never repairs the live source.
}
} // namespace

void VerifyMvrLayerUuidIdentity() {
  const auto directory = fs::temp_directory_path() / "perastage-layer-uuid-regression";
  fs::remove_all(directory);
  fs::create_directories(directory);
  TestAuthoredLayers(directory);
  TestDomainOperations(directory);
  TestLegacyAndDefensiveRecovery();
  fs::remove_all(directory);
}
