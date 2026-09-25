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

  const MvrInspectionResult fromFile = InspectMvr(path);
  const MvrInspectionResult fromBytes = InspectMvrBytes(bytes);
  assert(fromFile.Success() && fromBytes.Success());
  assert(fromFile.snapshot && fromBytes.snapshot);
  assert(fromFile.packageInventory && fromBytes.packageInventory);
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
  fs::remove_all(root);
}

// Verifies an unnamed authored Layer remains visible to inspection by UUID.
void TestUnnamedAuthoredLayer() {
  const std::string layerUuid = "10000000-0000-4000-8000-000000000010";
  const std::string fixtureUuid = "20000000-0000-4000-8000-000000000010";
  const std::string xml =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"><Scene>"
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
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"><Scene>"
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
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"><Scene>"
      "<Layers><Layer uuid=\"10000000-0000-4000-8000-000000000001\" "
      "name=\"Layer\"><ChildList><Fixture "
      "uuid=\"20000000-0000-4000-8000-000000000001\" name=\"Fixture\">"
      "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
      "<GDTFSpec>missing.gdtf</GDTFSpec></Fixture></ChildList></Layer>"
      "</Layers></Scene></GeneralSceneDescription>";

  const MvrInspectionResult auxiliaryCollision =
      InspectMvrBytes(BuildArchive({{"GeneralSceneDescription.xml", xml},
                                    {"fixture.gdtf", "first"},
                                    {"Fixture.gdtf", "second"}}));
  assert(auxiliaryCollision.Success());
  assert(HasCode(auxiliaryCollision,
                 "mvr.package.case_colliding_mvr_archive_entry"));
  assert(HasCode(auxiliaryCollision, "mvr.resource.missing_packaged_resource"));

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
  TestStandaloneInspectionParity();
  TestUnnamedAuthoredLayer();
  TestRecoveredUuidLayerOwnership();
  TestStructuralDiagnosticSummaries();
  TestPackageAndResourceDiagnostics();
  TestNonMvrPackageKind();
  return 0;
}
