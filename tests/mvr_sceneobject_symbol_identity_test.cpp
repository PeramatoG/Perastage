/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "bounds_cache_system.h"
#include "configmanager.h"
#include "matrixutils.h"
#include "mvrimporter.h"
#include "resource_sync_system.h"

namespace fs = std::filesystem;

// Writes a string entry into an open zip stream.
static void WriteZipEntry(wxZipOutputStream &zip, const std::string &name,
                          const std::string &contents) {
  assert(zip.PutNextEntry(name));
  zip.Write(contents.data(), contents.size());
}

// Creates a small MVR scene with repeated child Symbol UUIDs under distinct
// SceneObjects.
static fs::path WriteDuplicateSymbolSceneMvr(const fs::path &dir) {
  const fs::path mvrPath = dir / "duplicate_sceneobject_symbols.mvr";
  wxFileOutputStream out(mvrPath.generic_string());
  assert(out.IsOk());
  wxZipOutputStream zip(out);

  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Test\" providerVersion=\"1\">"
      "<Scene>"
      "<AUXData>"
      "<Symdef uuid=\"0334726B-F8ED-11EE-9EEC-48F17FC77B89\" name=\"FD34-L90\">"
      "<ChildList><Geometry3D fileName=\"eurotruss_FD34-L90.3ds\" "
      "/></ChildList>"
      "</Symdef>"
      "<Symdef uuid=\"0334726C-F8ED-11EE-9EEC-48F17FC77B89\" name=\"FD34-250\">"
      "<ChildList><Geometry3D fileName=\"eurotruss_FD34-250.3ds\" "
      "/></ChildList>"
      "</Symdef>"
      "</AUXData>"
      "<Layers><Layer uuid=\"layer-1\" name=\"Default\"><ChildList>"
      "<SceneObject uuid=\"0D492000-BD81-4D3F-17AA-9FDC8BB0D613\" "
      "name=\"eurotruss_FD34-250 1\">"
      "<Matrix>{1,0,0}{0,1,0}{0,0,1}{-3299.999952,-1200.000048,6000}</Matrix>"
      "<Geometries><Symbol uuid=\"418407C9-F77F-0000-0000-000000000000\" "
      "symdef=\"0334726C-F8ED-11EE-9EEC-48F17FC77B89\" /></Geometries>"
      "</SceneObject>"
      "<SceneObject uuid=\"0D492000-CC37-2169-ADA4-CCD28BB0D613\" "
      "name=\"eurotruss_FD34-250 3\">"
      "<Matrix>{1,0,0}{0,1,0}{0,0,1}{3299.999952,-1200.000048,6000}</Matrix>"
      "<Geometries><Symbol uuid=\"418407C9-F77F-0000-0000-000000000000\" "
      "symdef=\"0334726C-F8ED-11EE-9EEC-48F17FC77B89\" /></Geometries>"
      "</SceneObject>"
      "<SceneObject uuid=\"0D492000-AAAA-4D3F-17AA-9FDC8BB0D613\" "
      "name=\"eurotruss_FD34-L90 1\">"
      "<Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,0}</Matrix>"
      "<Geometries><Symbol uuid=\"418407C9-F77F-0000-0000-000000000000\" "
      "symdef=\"0334726B-F8ED-11EE-9EEC-48F17FC77B89\" /></Geometries>"
      "</SceneObject>"
      "</ChildList></Layer></Layers>"
      "</Scene></GeneralSceneDescription>";

  WriteZipEntry(zip, "GeneralSceneDescription.xml", xml);
  WriteZipEntry(zip, "eurotruss_FD34-250.3ds", "mesh-250");
  WriteZipEntry(zip, "eurotruss_FD34-L90.3ds", "mesh-l90");
  zip.Close();
  return mvrPath;
}

// Finds a scene object by display name in the imported scene.
static const SceneObject &FindObjectByName(const MvrScene &scene,
                                           const std::string &name) {
  for (const auto &[uuid, object] : scene.sceneObjects) {
    (void)uuid;
    if (object.name == name)
      return object;
  }
  assert(false && "scene object not found");
  return scene.sceneObjects.begin()->second;
}

// Adds an axis-aligned box mesh in millimeters to the loaded mesh cache.
static void AddBoxMesh(ResourceSyncState &state, const std::string &path,
                       float x, float y, float z) {
  Mesh mesh;
  mesh.vertices = {0.0f, 0.0f, 0.0f, x,    0.0f, 0.0f,
                   0.0f, y,    0.0f, 0.0f, 0.0f, z};
  mesh.indices = {0, 1, 2, 0, 2, 3};
  state.loadedMeshes[path] = std::move(mesh);
}

// Computes object bounds for a small imported scene using the viewer bounds
// cache path.
static std::unordered_map<std::string, Viewer3DBoundingBox>
BuildObjectBounds(const std::unordered_map<std::string, SceneObject> &objects) {
  ResourceSyncState resourceState;
  std::unordered_map<std::string, Viewer3DBoundingBox> modelBounds;
  std::unordered_map<std::string, Viewer3DBoundingBox> fixtureBounds;
  std::unordered_map<std::string, Viewer3DBoundingBox> trussBounds;
  std::unordered_map<std::string, Viewer3DBoundingBox> objectBounds;
  std::unordered_set<std::string> boundsHiddenLayers;
  size_t cachedVersion = 0;
  bool sceneChangedDirty = true;
  bool assetsChangedDirty = true;
  bool visibilityChangedDirty = true;
  std::mutex sortedListsMutex;
  bool sortedListsDirty = false;

  for (const auto &[uuid, object] : objects) {
    (void)uuid;
    for (const GeometryInstance &geometry : object.geometries) {
      ResourceSyncState::PathResolutionEntry entry;
      entry.resolvedPath = geometry.modelFile;
      entry.attempted = true;
      resourceState.resolvedModelRefs[geometry.modelFile] = entry;
    }
  }
  AddBoxMesh(resourceState, "eurotruss_FD34-250.3ds", 290.08f, 2500.0f,
             290.08f);
  AddBoxMesh(resourceState, "eurotruss_FD34-L90.3ds", 580.0f, 580.0f, 290.08f);

  BoundsCacheSystem::Context context{resourceState,
                                     modelBounds,
                                     fixtureBounds,
                                     trussBounds,
                                     objectBounds,
                                     boundsHiddenLayers,
                                     1,
                                     cachedVersion,
                                     sceneChangedDirty,
                                     assetsChangedDirty,
                                     visibilityChangedDirty,
                                     sortedListsMutex,
                                     sortedListsDirty};
  std::unordered_set<std::string> hiddenLayers;
  std::unordered_map<std::string, Truss> trusses;
  std::unordered_map<std::string, Fixture> fixtures;
  BoundsCacheSystem::RebuildIfDirty(context, hiddenLayers, trusses, objects,
                                    fixtures);
  return objectBounds;
}

// Returns the size of one axis from a viewer bounding box.
static float BoundsSize(const Viewer3DBoundingBox &bounds, size_t axis) {
  return bounds.max[axis] - bounds.min[axis];
}

// Runs the duplicate SceneObject Symbol identity regression test.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  ConfigManager::Get().Reset();
  const fs::path tempDir =
      fs::temp_directory_path() / "mvr_sceneobject_symbol_identity_test";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir);

  MvrImporter importer;
  MvrImportResult result;
  assert(importer.ImportFromFile(
      WriteDuplicateSymbolSceneMvr(tempDir).generic_string(), result,
      MvrImportMode::ParseOnly, false, false));

  const MvrScene &scene = result.scene;
  assert(scene.sceneObjects.size() == 3);
  const SceneObject &fd250A = FindObjectByName(scene, "eurotruss_FD34-250 1");
  const SceneObject &fd250B = FindObjectByName(scene, "eurotruss_FD34-250 3");
  const SceneObject &l90 = FindObjectByName(scene, "eurotruss_FD34-L90 1");

  assert(fd250A.geometries.size() == 1);
  assert(fd250B.geometries.size() == 1);
  assert(l90.geometries.size() == 1);
  assert(fs::path(fd250A.geometries.front().modelFile).filename() ==
         "eurotruss_FD34-250.3ds");
  assert(fs::path(fd250B.geometries.front().modelFile).filename() ==
         "eurotruss_FD34-250.3ds");
  assert(fd250A.geometries.front().modelFile ==
         fd250B.geometries.front().modelFile);
  assert(fd250A.geometries.front().sourceSymbolUuid ==
         fd250B.geometries.front().sourceSymbolUuid);
  assert(l90.geometries.front().sourceSymbolUuid ==
         fd250A.geometries.front().sourceSymbolUuid);
  assert(l90.geometries.front().sourceSymdefUuid !=
         fd250A.geometries.front().sourceSymdefUuid);
  assert(fd250A.geometries.front().instanceKey !=
         fd250B.geometries.front().instanceKey);
  assert(l90.geometries.front().instanceKey !=
         fd250A.geometries.front().instanceKey);
  assert(MatrixUtils::FormatMatrix(fd250A.geometries.front().localTransform) ==
         MatrixUtils::FormatMatrix(fd250B.geometries.front().localTransform));
  assert(fd250A.transform.o[0] != fd250B.transform.o[0]);
  assert(fd250A.transform.o[1] == fd250B.transform.o[1]);
  assert(fd250A.transform.o[2] == fd250B.transform.o[2]);

  const auto bounds = BuildObjectBounds(scene.sceneObjects);
  const auto &boundsA = bounds.at(fd250A.uuid);
  const auto &boundsB = bounds.at(fd250B.uuid);
  constexpr float kTolerance = 1e-5f;
  for (size_t axis = 0; axis < 3; ++axis) {
    assert(std::abs(BoundsSize(boundsA, axis) - BoundsSize(boundsB, axis)) <
           kTolerance);
  }

  fs::remove_all(tempDir);
  return 0;
}
