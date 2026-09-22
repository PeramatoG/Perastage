#include "inspection/mvr_inspection.h"

#include "support/archive_entry_test_utils.h"

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
       {"models/object.glb", "geometry"}},
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
      "provider=\"Standalone\" providerVersion=\"1\"><Scene><Layers>"
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
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
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
  assert(ReadBytes(path) == bytes);
  fs::remove_all(root);
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
  TestPackageAndResourceDiagnostics();
  TestNonMvrPackageKind();
  return 0;
}
