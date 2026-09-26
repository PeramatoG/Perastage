#include "inspection/mvr_inspection.h"

#include "mvr_import_package.h"
#include "mvr_read_service.h"

#include "support/archive_entry_test_utils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace perastage::inspection;

namespace {

// Reads one test package without changing its contents.
std::vector<std::uint8_t> ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Writes one deterministic stored MVR used by both public entry points.
void WriteMvr(const fs::path &path, const std::string &xml) {
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(
      path,
      {{"GeneralSceneDescription.xml", xml},
       {"fixture.gdtf", "embedded"},
       {"models/object.glb", "geometry"},
       {"models/a.3ds", "symdef-a"},
       {"models/z.3ds", "symdef-z"}},
      error));
  assert(error.empty());
}

// Creates deterministic archive bytes with the requested raw entry order.
std::vector<std::uint8_t>
BuildArchive(const std::vector<std::pair<std::string, std::string>> &entries) {
  const fs::path path =
      fs::temp_directory_path() / "perastage-mvr-inspection-buffer.zip";
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, entries, error));
  assert(error.empty());
  std::vector<std::uint8_t> bytes = ReadBytes(path);
  fs::remove(path);
  return bytes;
}

// Reports whether an inspection result contains one stable diagnostic code.
bool HasCode(const MvrInspectionResult &result, const std::string &code) {
  for (const Diagnostic &diagnostic : result.inspection.diagnostics) {
    if (diagnostic.code == code)
      return true;
  }
  return false;
}

// Reports whether any inspection or validation finding has a classification.
bool HasClassification(const MvrInspectionResult &result,
                       DiagnosticClassification classification) {
  const auto matches = [classification](const Diagnostic &diagnostic) {
    return diagnostic.classification == classification;
  };
  if (std::any_of(result.inspection.diagnostics.begin(),
                  result.inspection.diagnostics.end(), matches))
    return true;
  for (const ValidationResult &validation : result.validation) {
    if (std::any_of(validation.diagnostics.begin(),
                    validation.diagnostics.end(), matches))
      return true;
  }
  return false;
}

// Compares every stable field of two inspection diagnostics.
void AssertDiagnosticEqual(const Diagnostic &left, const Diagnostic &right) {
  assert(left.severity == right.severity);
  assert(left.domain == right.domain);
  assert(left.classification == right.classification);
  assert(left.code == right.code);
  assert(left.message == right.message);
  assert(left.location.has_value() == right.location.has_value());
  if (left.location) {
    assert(left.location->sourcePath == right.location->sourcePath);
    assert(left.location->packageEntry == right.location->packageEntry);
    assert(left.location->xmlPath == right.location->xmlPath);
    assert(left.location->line == right.location->line);
    assert(left.location->column == right.location->column);
  }
}

// Compares the complete stable public MVR inspection projection.
void AssertStableResultEqual(const MvrInspectionResult &left,
                             const MvrInspectionResult &right) {
  assert(left.Success() == right.Success());
  assert(left.inspection.request.sourcePath ==
         right.inspection.request.sourcePath);
  assert(left.inspection.diagnostics.size() ==
         right.inspection.diagnostics.size());
  for (std::size_t index = 0; index < left.inspection.diagnostics.size();
       ++index)
    AssertDiagnosticEqual(left.inspection.diagnostics[index],
                          right.inspection.diagnostics[index]);
  assert(left.packageInventory.has_value() ==
         right.packageInventory.has_value());
  if (left.packageInventory) {
    assert(left.packageInventory->kind == right.packageInventory->kind);
    assert(left.packageInventory->canonicalRootDocumentPresent ==
           right.packageInventory->canonicalRootDocumentPresent);
    assert(left.packageInventory->entries.size() ==
           right.packageInventory->entries.size());
    for (std::size_t index = 0; index < left.packageInventory->entries.size();
         ++index) {
      const PackageEntry &a = left.packageInventory->entries[index];
      const PackageEntry &b = right.packageInventory->entries[index];
      assert(a.displayPath == b.displayPath);
      assert(a.normalizedPath == b.normalizedPath);
      assert(a.extension == b.extension);
      assert(a.type == b.type);
      assert(a.uncompressedSize == b.uncompressedSize);
      assert(a.sizeKnown == b.sizeKnown);
      assert(a.pathSafe == b.pathSafe);
    }
  }
  assert(left.validation.size() == right.validation.size());
  for (std::size_t index = 0; index < left.validation.size(); ++index) {
    assert(left.validation[index].layer == right.validation[index].layer);
    assert(left.validation[index].status == right.validation[index].status);
    assert(left.validation[index].schema.has_value() ==
           right.validation[index].schema.has_value());
    if (left.validation[index].schema) {
      assert(left.validation[index].schema->format ==
             right.validation[index].schema->format);
      assert(left.validation[index].schema->formatVersion ==
             right.validation[index].schema->formatVersion);
      assert(left.validation[index].schema->schemaVersion ==
             right.validation[index].schema->schemaVersion);
      assert(left.validation[index].schema->provenance ==
             right.validation[index].schema->provenance);
      assert(left.validation[index].schema->sourceRevision ==
             right.validation[index].schema->sourceRevision);
    }
    assert(left.validation[index].diagnostics.size() ==
           right.validation[index].diagnostics.size());
    for (std::size_t diagnostic = 0;
         diagnostic < left.validation[index].diagnostics.size(); ++diagnostic)
      AssertDiagnosticEqual(left.validation[index].diagnostics[diagnostic],
                            right.validation[index].diagnostics[diagnostic]);
  }
  assert(left.snapshot.has_value() == right.snapshot.has_value());
  if (!left.snapshot)
    return;
  const MvrInspectionSnapshot &a = *left.snapshot;
  const MvrInspectionSnapshot &b = *right.snapshot;
  assert(a.versionMajor == b.versionMajor && a.versionMinor == b.versionMinor);
  assert(a.provider == b.provider && a.providerVersion == b.providerVersion);
  assert(a.sceneDescriptionEntry == b.sceneDescriptionEntry);
  assert(a.sceneDescriptionXml == b.sceneDescriptionXml);
  assert(a.embeddedGdtfEntries == b.embeddedGdtfEntries);
  assert(a.referencedResources == b.referencedResources);
  assert(a.layers == b.layers);
  assert(a.fixtures == b.fixtures);
  assert(a.trusses == b.trusses);
  assert(a.supports == b.supports);
  assert(a.sceneObjects == b.sceneObjects);
  assert(a.groupObjects == b.groupObjects);
  assert(a.positions == b.positions);
  assert(a.symdefs == b.symdefs);
  assert(a.foreignUserData == b.foreignUserData);
  assert(a.nodeCounts == b.nodeCounts);
}

// Returns one validation stage from an MVR inspection result.
const ValidationResult &Validation(const MvrInspectionResult &result,
                                   ValidationLayer layer) {
  const auto found = std::find_if(
      result.validation.begin(), result.validation.end(),
      [layer](const auto &validation) { return validation.layer == layer; });
  assert(found != result.validation.end());
  return *found;
}

// Returns a minimal MVR 1.6 document accepted by both compared schemas.
std::string StandardsValidXml() {
  return "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
         "provider=\"Perastage\" providerVersion=\"1.7\">"
         "<UserData><Data provider=\"Example\"/></UserData><Scene><Layers>"
         "<Layer uuid=\"10000000-0000-4000-8000-000000000001\" "
         "name=\"Main\"><ChildList/></Layer></Layers></Scene>"
         "</GeneralSceneDescription>";
}

// Verifies MVR root requirements and independent XML/schema outcomes.
void TestValidationLayers() {
  const MvrInspectionResult valid = InspectMvrBytes(
      BuildArchive({{"GeneralSceneDescription.xml", StandardsValidXml()}}));
  assert(valid.Success());
  assert(Validation(valid, ValidationLayer::XmlWellFormedness).status ==
         ValidationStatus::Valid);
  assert(Validation(valid, ValidationLayer::Schema).status ==
         ValidationStatus::Valid);
  assert(!HasClassification(valid, DiagnosticClassification::Compatibility));

  const std::string missingScene =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Perastage\" providerVersion=\"1.7\"/>";
  const MvrInspectionResult invalid = InspectMvrBytes(
      BuildArchive({{"GeneralSceneDescription.xml", missingScene}}));
  assert(Validation(invalid, ValidationLayer::XmlWellFormedness).status ==
         ValidationStatus::Valid);
  assert(Validation(invalid, ValidationLayer::Schema).status ==
         ValidationStatus::Invalid);

  const std::string wrongOrder =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Perastage\" providerVersion=\"1.7\"><Scene><Layers/>"
      "</Scene><UserData/></GeneralSceneDescription>";
  const MvrInspectionResult ordered = InspectMvrBytes(
      BuildArchive({{"GeneralSceneDescription.xml", wrongOrder}}));
  assert(Validation(ordered, ValidationLayer::Schema).status ==
         ValidationStatus::Invalid);

  const MvrInspectionResult malformed = InspectMvrBytes(BuildArchive(
      {{"GeneralSceneDescription.xml", "<GeneralSceneDescription>"}}));
  assert(Validation(malformed, ValidationLayer::XmlWellFormedness).status ==
         ValidationStatus::Invalid);
  assert(Validation(malformed, ValidationLayer::Schema).status ==
         ValidationStatus::NotRun);

  const MvrInspectionResult compatible = InspectMvrBytes(
      BuildArchive({{"generalscenedescription.xml", StandardsValidXml()}}));
  assert(Validation(compatible, ValidationLayer::Schema).status ==
         ValidationStatus::Valid);
  assert(HasCode(compatible, "mvr.package.non_canonical_scene_description"));
  assert(
      HasClassification(compatible, DiagnosticClassification::Compatibility));

  const std::string mixedXml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"><Scene>"
      "<Layers/></Scene></GeneralSceneDescription>";
  const MvrInspectionResult mixed = InspectMvrBytes(
      BuildArchive({{"generalscenedescription.xml", mixedXml}}));
  assert(HasClassification(mixed, DiagnosticClassification::Compatibility));
  assert(HasClassification(mixed, DiagnosticClassification::Standards));

  const std::string semanticWarning =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Perastage\" providerVersion=\"1.7\"><Scene><Layers>"
      "<Layer uuid=\"10000000-0000-4000-8000-000000000001\"><ChildList>"
      "<Fixture uuid=\"20000000-0000-4000-8000-000000000001\">"
      "<GDTFSpec>missing.gdtf</GDTFSpec><FixtureID>1</FixtureID>"
      "<UnitNumber>1</UnitNumber><ChildList/></Fixture></ChildList></Layer>"
      "</Layers></Scene></GeneralSceneDescription>";
  const MvrInspectionResult semantic = InspectMvrBytes(
      BuildArchive({{"GeneralSceneDescription.xml", semanticWarning}}));
  assert(Validation(semantic, ValidationLayer::Schema).status ==
         ValidationStatus::Valid);
  assert(
      Validation(semantic, ValidationLayer::SemanticInteroperability).status ==
      ValidationStatus::Valid);
  assert(HasCode(semantic, "mvr.resource.missing_packaged_resource"));

  const std::string missingProvider =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"><Scene>"
      "<Layers/></Scene></GeneralSceneDescription>";
  const MvrInspectionResult providerSemantic = InspectMvrBytes(
      BuildArchive({{"GeneralSceneDescription.xml", missingProvider}}));
  assert(Validation(providerSemantic, ValidationLayer::Schema).status ==
         ValidationStatus::Valid);
  assert(Validation(providerSemantic, ValidationLayer::SemanticInteroperability)
             .status == ValidationStatus::Invalid);
  assert(HasCode(providerSemantic, "mvr.semantic.missing_provider"));
  assert(HasCode(providerSemantic, "mvr.semantic.missing_provider_version"));

  const std::string fixtureRules =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Perastage\" providerVersion=\"1.7\"><Scene><Layers>"
      "<Layer uuid=\"10000000-0000-4000-8000-000000000001\"><ChildList>"
      "<Fixture uuid=\"20000000-0000-4000-8000-000000000001\">"
      "<FixtureID>1</FixtureID><FixtureTypeId>7</FixtureTypeId>"
      "<UnitNumber>1</UnitNumber></Fixture></ChildList></Layer>"
      "</Layers></Scene></GeneralSceneDescription>";
  const MvrInspectionResult fixtureSemantic = InspectMvrBytes(
      BuildArchive({{"GeneralSceneDescription.xml", fixtureRules}}));
  assert(Validation(fixtureSemantic, ValidationLayer::Schema).status ==
         ValidationStatus::Valid);
  assert(Validation(fixtureSemantic, ValidationLayer::SemanticInteroperability)
             .status == ValidationStatus::Invalid);
  assert(HasCode(fixtureSemantic, "mvr.semantic.fixture_missing_child_list"));
  assert(HasCode(fixtureSemantic, "mvr.semantic.fixture_type_id_not_allowed"));
}

// Verifies deterministic failure and immutability for malformed MVR containers.
void TestMalformedOwnedInputMatrix() {
  const std::vector<std::vector<std::uint8_t>> corpus = {
      {},
      {'n', 'o', 't', '-', 'z', 'i', 'p'},
      {'P', 'K', 3, 4, 0, 0, 0, 0},
  };
  for (std::size_t index = 0; index < corpus.size(); ++index) {
    const Request request{
        fs::path("malformed-" + std::to_string(index) + ".mvr")};
    const std::vector<std::uint8_t> before = corpus[index];
    const MvrInspectionResult first = InspectMvrBytes(corpus[index], request);
    const MvrInspectionResult second = InspectMvrBytes(corpus[index], request);
    assert(!first.Success());
    assert(first.inspection.HasFatalDiagnostics());
    AssertStableResultEqual(first, second);
    assert(corpus[index] == before);
  }
}

// Finds one stable summary count by node type.
std::size_t Count(const MvrInspectionSnapshot &snapshot,
                  const std::string &type) {
  for (const MvrNodeCount &count : snapshot.nodeCounts) {
    if (count.type == type)
      return count.count;
  }
  return 0;
}

// Verifies that the standalone target exposes equivalent file and byte facts.
void TestStandaloneInspectionParity() {
  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Standalone\" providerVersion=\"1\">"
      "<UserData><Data provider=\"Foreign\" ver=\"2\"><Value raw=\"yes\"/>"
      "</Data></UserData><Scene><AUXData>"
      "<Position uuid=\"70000000-0000-4000-8000-000000000001\" name=\"FOH\"/>"
      "<Symdef uuid=\"80000000-0000-4000-8000-000000000001\">"
      "<ChildList><Geometry3D fileName=\"models/z.3ds\"/>"
      "<Geometry3D fileName=\"models/a.3ds\"/></ChildList></Symdef>"
      "<Symdef uuid=\"80000000-0000-4000-8000-000000000002\" "
      "geometryType=\"Other\"/>"
      "</AUXData><Layers>"
      "<Layer uuid=\"10000000-0000-4000-8000-000000000001\" name=\"Main\">"
      "<ChildList><Fixture uuid=\"20000000-0000-4000-8000-000000000001\" "
      "name=\"Fixture\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<GDTFSpec>fixture.gdtf</GDTFSpec></Fixture>"
      "<Truss uuid=\"50000000-0000-4000-8000-000000000001\" name=\"Truss\">"
      "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><Geometries>"
      "<Geometry3D fileName=\"models/object.glb\"/></Geometries></Truss>"
      "<Support uuid=\"60000000-0000-4000-8000-000000000001\" name=\"Hoist\">"
      "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix><ChainLength>1</ChainLength>"
      "<Geometries/></Support>"
      "<GroupObject uuid=\"30000000-0000-4000-8000-000000000001\" "
      "name=\"Group\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<ChildList><SceneObject uuid=\"40000000-0000-4000-8000-000000000001\" "
      "name=\"Object\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<Geometries><Geometry3D fileName=\"models/object.glb\"/>"
      "</Geometries></SceneObject></ChildList></GroupObject>"
      "</ChildList></Layer>"
      "<Layer uuid=\"10000000-0000-4000-8000-000000000002\" name=\"Main\">"
      "<ChildList><Fixture uuid=\"20000000-0000-4000-8000-000000000002\" "
      "name=\"Fixture 2\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<GDTFSpec>fixture.gdtf</GDTFSpec></Fixture></ChildList></Layer>"
      "</Layers></Scene></GeneralSceneDescription>";
  const fs::path root = fs::temp_directory_path() / "perastage-mvr-inspection";
  fs::create_directories(root);
  const fs::path path = root / "scene.mvr";
  WriteMvr(path, xml);
  const std::vector<std::uint8_t> bytes = ReadBytes(path);
  const std::vector<std::uint8_t> originalBytes = bytes;

  const MvrInspectionResult fromFile = InspectMvr(path);
  const Request logicalRequest{path};
  const MvrInspectionResult fromBytes = InspectMvrBytes(bytes, logicalRequest);
  const MvrInspectionResult repeated = InspectMvrBytes(bytes, logicalRequest);
  assert(fromFile.Success() && fromBytes.Success());
  assert(fromFile.snapshot && fromBytes.snapshot);
  assert(fromFile.packageInventory && fromBytes.packageInventory);
  AssertStableResultEqual(fromFile, fromBytes);
  AssertStableResultEqual(fromBytes, repeated);
  assert(fromFile.packageInventory->entries.size() ==
         fromBytes.packageInventory->entries.size());
  for (std::size_t index = 0; index < fromFile.packageInventory->entries.size();
       ++index) {
    const PackageEntry &left = fromFile.packageInventory->entries[index];
    const PackageEntry &right = fromBytes.packageInventory->entries[index];
    assert(left.displayPath == right.displayPath);
    assert(left.normalizedPath == right.normalizedPath);
    assert(left.uncompressedSize == right.uncompressedSize);
    assert(left.pathSafe == right.pathSafe);
  }
  assert(fromFile.snapshot->sceneDescriptionXml == xml);
  assert(fromFile.snapshot->sceneDescriptionXml ==
         fromBytes.snapshot->sceneDescriptionXml);
  assert(fromFile.snapshot->fixtures == fromBytes.snapshot->fixtures);
  assert(fromFile.snapshot->trusses == fromBytes.snapshot->trusses);
  assert(fromFile.snapshot->supports == fromBytes.snapshot->supports);
  assert(fromFile.snapshot->sceneObjects == fromBytes.snapshot->sceneObjects);
  assert(fromFile.snapshot->groupObjects == fromBytes.snapshot->groupObjects);
  assert(fromFile.snapshot->nodeCounts == fromBytes.snapshot->nodeCounts);
  assert(fromFile.snapshot->fixtures.front().layerUuid ==
         "10000000-0000-4000-8000-000000000001");
  assert(fromFile.snapshot->fixtures.front().layerName == "Main");
  assert(fromFile.snapshot->fixtures.back().layerUuid ==
         "10000000-0000-4000-8000-000000000002");
  assert(fromFile.snapshot->layers.size() == 2);
  assert(fromFile.snapshot->layers.front().childUuids ==
         (std::vector<std::string>{"20000000-0000-4000-8000-000000000001",
                                   "30000000-0000-4000-8000-000000000001",
                                   "50000000-0000-4000-8000-000000000001",
                                   "60000000-0000-4000-8000-000000000001"}));
  assert(fromFile.snapshot->layers.back().childUuids ==
         std::vector<std::string>{"20000000-0000-4000-8000-000000000002"});
  assert(Count(*fromFile.snapshot, "fixtures") ==
         fromFile.snapshot->fixtures.size());
  assert(Count(*fromFile.snapshot, "trusses") == 1);
  assert(Count(*fromFile.snapshot, "supports") == 1);
  assert(Count(*fromFile.snapshot, "group_objects") ==
         fromFile.snapshot->groupObjects.size());
  assert(fromFile.snapshot->groupObjects.size() == 1);
  assert(fromFile.snapshot->groupObjects.front().childUuids ==
         std::vector<std::string>{"40000000-0000-4000-8000-000000000001"});
  assert(fromFile.snapshot->sceneObjects.front().parentGroupUuid ==
         fromFile.snapshot->groupObjects.front().uuid);
  assert(fromFile.snapshot->sceneObjects.front().layerUuid ==
         "10000000-0000-4000-8000-000000000001");
  assert(Count(*fromFile.snapshot, "positions") == 1);
  assert(Count(*fromFile.snapshot, "symdefs") == 2);
  assert(fromFile.snapshot->symdefs.front().uuid ==
         "80000000-0000-4000-8000-000000000001");
  assert(fromFile.snapshot->symdefs.front().resourceReferences ==
         (std::vector<std::string>{"models/a.3ds", "models/z.3ds"}));
  assert(fromFile.snapshot->symdefs.back().uuid ==
         "80000000-0000-4000-8000-000000000002");
  assert(fromFile.snapshot->symdefs.back().resourceReferences.empty());
  const auto hasSymdefResource = [&](const std::string &path) {
    return std::find(fromFile.snapshot->referencedResources.begin(),
                     fromFile.snapshot->referencedResources.end(),
                     MvrResourceReference{"geometry", path}) !=
           fromFile.snapshot->referencedResources.end();
  };
  assert(hasSymdefResource("models/a.3ds"));
  assert(hasSymdefResource("models/z.3ds"));
  assert(fromFile.snapshot->foreignUserData.size() == 1);
  assert(fromFile.snapshot->foreignUserData.front().provider == "Foreign");
  assert(fromFile.snapshot->foreignUserData.front().version == "2");
  assert(fromFile.snapshot->foreignUserData.front().xml ==
         "<Data provider=\"Foreign\" ver=\"2\"><Value raw=\"yes\"/></Data>");
  assert(ReadBytes(path) == bytes);
  assert(bytes == originalBytes);
  fs::remove_all(root);
}

// Verifies an unnamed authored Layer remains visible to inspection by UUID.
void TestUnnamedAuthoredLayer() {
  const std::string layerUuid = "10000000-0000-4000-8000-000000000010";
  const std::string fixtureUuid = "20000000-0000-4000-8000-000000000010";
  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Test\" providerVersion=\"1\"><Scene>"
      "<Layers><Layer uuid=\"" +
      layerUuid + "\"><ChildList><Fixture uuid=\"" + fixtureUuid +
      "\" name=\"Fixture\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "</Fixture></ChildList></Layer></Layers></Scene>"
      "</GeneralSceneDescription>";

  const MvrInspectionResult inspected =
      InspectMvrBytes(BuildArchive({{"GeneralSceneDescription.xml", xml}}));
  assert(inspected.Success() && inspected.snapshot);
  assert(inspected.snapshot->layers.size() == 1);
  const MvrSceneNodeDescriptor &layer = inspected.snapshot->layers.front();
  assert(layer.uuid == layerUuid);
  assert(layer.name.empty());
  assert(inspected.snapshot->fixtures.size() == 1);
  const MvrSceneNodeDescriptor &fixture = inspected.snapshot->fixtures.front();
  assert(layer.childUuids == std::vector<std::string>{fixture.uuid});
  assert(fixture.uuid == fixtureUuid);
  assert(fixture.layerUuid == layerUuid);
  assert(fixture.layerName.empty());
}

// Verifies recovered node identities remain authoritative across hierarchy
// views.
void TestRecoveredUuidLayerOwnership() {
  const std::string repeatedUuid = "20000000-0000-4000-8000-000000000001";
  const std::string firstLayer = "10000000-0000-4000-8000-000000000001";
  const std::string secondLayer = "10000000-0000-4000-8000-000000000002";
  const std::string groupUuid = "30000000-0000-4000-8000-000000000001";
  const std::string identity = "1,0,0,0,1,0,0,0,1,0,0,0";
  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Test\" providerVersion=\"1\"><Scene>"
      "<Layers><Layer uuid=\"" +
      firstLayer +
      "\" name=\"Shared\">"
      "<ChildList><Fixture uuid=\"" +
      repeatedUuid + "\" name=\"Fixture\"><Matrix>" + identity +
      "</Matrix></Fixture></ChildList></Layer>"
      "<Layer uuid=\"" +
      secondLayer +
      "\" name=\"Shared\"><ChildList>"
      "<Truss uuid=\"" +
      repeatedUuid + "\" name=\"Recovered Truss\"><Matrix>" + identity +
      "</Matrix><Geometries/></Truss><GroupObject uuid=\"" + groupUuid +
      "\" name=\"Group\"><Matrix>" + identity +
      "</Matrix><ChildList><SceneObject uuid=\"" + repeatedUuid +
      "\" name=\"Recovered Object\"><Matrix>" + identity +
      "</Matrix><Geometries/></SceneObject></ChildList></GroupObject>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  const std::vector<std::uint8_t> bytes =
      BuildArchive({{"GeneralSceneDescription.xml", xml}});

  std::vector<MvrImportDiagnostic> diagnostics;
  std::optional<mvr::ImportPackage> package =
      mvr::AcquireImportPackage(bytes, diagnostics);
  assert(package);
  MvrImportResult parsed;
  mvr::MvrReadContext context;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;
  assert(mvr::ReadAcquiredMvrPackage(*package, parsed, options, nullptr,
                                     &context));
  assert(parsed.scene.fixtures.size() == 1);
  assert(parsed.scene.trusses.size() == 1);
  assert(parsed.scene.sceneObjects.size() == 1);
  const std::string fixtureUuid = parsed.scene.fixtures.begin()->first;
  const std::string trussUuid = parsed.scene.trusses.begin()->first;
  const std::string objectUuid = parsed.scene.sceneObjects.begin()->first;
  assert(fixtureUuid != trussUuid);
  assert(fixtureUuid != objectUuid);
  assert(trussUuid != objectUuid);
  assert(context.layerUuidByNodeUuid.at(fixtureUuid) == firstLayer);
  assert(context.layerUuidByNodeUuid.at(trussUuid) == secondLayer);
  assert(context.layerUuidByNodeUuid.at(objectUuid) == secondLayer);
  assert(parsed.scene.groupObjects.at(groupUuid).children.front().uuid ==
         objectUuid);

  const MvrInspectionResult inspected = InspectMvrBytes(bytes);
  assert(inspected.Success() && inspected.snapshot);
  assert(inspected.snapshot->fixtures.front().uuid == fixtureUuid);
  assert(inspected.snapshot->fixtures.front().layerUuid == firstLayer);
  assert(inspected.snapshot->trusses.front().uuid == trussUuid);
  assert(inspected.snapshot->trusses.front().layerUuid == secondLayer);
  assert(inspected.snapshot->sceneObjects.front().uuid == objectUuid);
  assert(inspected.snapshot->sceneObjects.front().layerUuid == secondLayer);
  assert(inspected.snapshot->layers.front().childUuids ==
         std::vector<std::string>{fixtureUuid});
  std::vector<std::string> secondLayerChildren{groupUuid, trussUuid};
  std::sort(secondLayerChildren.begin(), secondLayerChildren.end());
  assert(inspected.snapshot->layers.back().childUuids == secondLayerChildren);
  assert(inspected.snapshot->groupObjects.front().childUuids ==
         std::vector<std::string>{objectUuid});
}

// Verifies structural reader diagnostics retain deterministic detail.
void TestStructuralDiagnosticSummaries() {
  const std::string symdefUuid = "80000000-0000-4000-8000-000000000001";
  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"><Scene>"
      "<AUXData><Symdef uuid=\"" +
      symdefUuid +
      "\"><ChildList><Geometry3D fileName=\"models/a.3ds\"/>"
      "</ChildList></Symdef></AUXData><Layers><Layer "
      "uuid=\"10000000-0000-4000-8000-000000000001\" name=\"Layer\">"
      "<ChildList><Truss uuid=\"50000000-0000-4000-8000-000000000001\" "
      "name=\"Truss\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<Geometries><Symbol symdef=\"" +
      symdefUuid +
      "\"/></Geometries></Truss><Support "
      "uuid=\"60000000-0000-4000-8000-000000000001\" name=\"Support\">"
      "<Matrix>0,0,0,0,0,0,0,0,0,0,0,0</Matrix><Geometries/></Support>"
      "<SceneObject uuid=\"40000000-0000-4000-8000-000000000001\" "
      "name=\"Object\"><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<Geometries><Geometry3D fileName=\"models/a.3ds\">"
      "<Matrix>0.001,0,0,0,0.001,0,0,0,0.001,0,0,0</Matrix>"
      "</Geometry3D></Geometries></SceneObject></ChildList></Layer>"
      "</Layers></Scene></GeneralSceneDescription>";
  const std::vector<std::uint8_t> bytes = BuildArchive(
      {{"GeneralSceneDescription.xml", xml}, {"models/a.3ds", "geometry"}});
  std::vector<MvrImportDiagnostic> diagnostics;
  std::optional<mvr::ImportPackage> package =
      mvr::AcquireImportPackage(bytes, diagnostics);
  assert(package);
  MvrImportResult parsed;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;
  std::vector<std::string> messages;
  assert(mvr::ReadAcquiredMvrPackage(
      *package, parsed, options, nullptr, nullptr, {},
      [&](mvr::MvrReadLogLevel, const std::string &message) {
        messages.push_back(message);
      }));
  const auto containsMessage = [&](const std::string &text) {
    return std::any_of(messages.begin(), messages.end(),
                       [&](const std::string &message) {
                         return message.find(text) != std::string::npos;
                       });
  };
  assert(containsMessage("tiny uniform geometry scales without warning. "
                         "Contexts: SceneObject/Geometry3D=1"));
  assert(containsMessage("suspicious matrices detected. Contexts: "
                         "Child/Support=1"));
  assert(containsMessage("MVR import suspicious matrix example: "
                         "Child/Support"));
  assert(containsMessage("Symdef counts: '" + symdefUuid + "'=1"));
}

// Verifies package safety, collision severity, and missing-resource reporting.
void TestPackageAndResourceDiagnostics() {
  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Perastage Test\" providerVersion=\"1.0\"><Scene>"
      "<Layers><Layer uuid=\"10000000-0000-4000-8000-000000000001\" "
      "name=\"Layer\"><ChildList><Fixture "
      "uuid=\"20000000-0000-4000-8000-000000000001\" name=\"Fixture\">"
      "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<GDTFSpec>missing.gdtf</GDTFSpec><ChildList/></Fixture>"
      "</ChildList></Layer>"
      "</Layers></Scene></GeneralSceneDescription>";

  const MvrInspectionResult auxiliaryCollision =
      InspectMvrBytes(BuildArchive({{"GeneralSceneDescription.xml", xml},
                                    {"fixture.gdtf", "first"},
                                    {"Fixture.gdtf", "second"}}));
  assert(auxiliaryCollision.Success());
  assert(HasCode(auxiliaryCollision,
                 "mvr.package.case_colliding_mvr_archive_entry"));
  assert(HasCode(auxiliaryCollision, "mvr.resource.missing_packaged_resource"));
  assert(Validation(auxiliaryCollision, ValidationLayer::Schema).status ==
         ValidationStatus::Invalid);
  assert(
      Validation(auxiliaryCollision, ValidationLayer::SemanticInteroperability)
          .status == ValidationStatus::Valid);

  const MvrInspectionResult rootCollision =
      InspectMvrBytes(BuildArchive({{"GeneralSceneDescription.xml", xml},
                                    {"generalscenedescription.xml", xml}}));
  assert(!rootCollision.Success());
  assert(
      HasCode(rootCollision, "mvr.package.case_colliding_mvr_archive_entry"));

  const MvrInspectionResult unsafe = InspectMvrBytes(BuildArchive(
      {{"GeneralSceneDescription.xml", xml}, {"../escape.gdtf", "unsafe"}}));
  assert(unsafe.Success());
  assert(HasCode(unsafe, package_diagnostic_codes::UnsafeEntryPath));
  assert(unsafe.packageInventory);
  assert(!unsafe.packageInventory->entries.back().pathSafe);
}

// Verifies that a supported non-MVR package receives a distinct input finding.
void TestNonMvrPackageKind() {
  const fs::path path = fs::temp_directory_path() / "inspection-input.gdtf";
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(
      path, {{"description.xml", "<GDTF/>"}}, error));
  const MvrInspectionResult result = InspectMvr(path);
  assert(!result.Success());
  assert(HasCode(result, "mvr.input.unsupported_package_kind"));
  fs::remove(path);
}

} // namespace

// Runs the standalone MVR inspection contract without application or GUI code.
int main() {
  TestValidationLayers();
  TestMalformedOwnedInputMatrix();
  TestStandaloneInspectionParity();
  TestUnnamedAuthoredLayer();
  TestRecoveredUuidLayerOwnership();
  TestStructuralDiagnosticSummaries();
  TestPackageAndResourceDiagnostics();
  TestNonMvrPackageKind();
  return 0;
}
