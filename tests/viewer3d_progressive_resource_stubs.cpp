#include "../viewer3d/gdtfloader.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "loader_obj.h"
#include "projectutils.h"
#include <filesystem>

size_t modelLoads = 0;
size_t fixtureLoads = 0;
namespace {
Mesh Triangle() {
  Mesh mesh;
  mesh.vertices = {0,0,0,1,0,0,0,1,0};
  mesh.indices = {0,1,2};
  return mesh;
}
}
bool Load3DS(const std::string &, Mesh &mesh, bool, std::string *, bool) {
  ++modelLoads; mesh = Triangle(); return true;
}
bool LoadGLB(const std::string &, Mesh &mesh, std::string *, bool) {
  ++modelLoads; mesh = Triangle(); return true;
}
bool LoadOBJ(const std::string &, Mesh &mesh, std::string *) {
  ++modelLoads; mesh = Triangle(); return true;
}
bool LoadGdtf(const std::string &, std::vector<GdtfObject> &objects,
              const std::string &, std::string *) {
  ++fixtureLoads;
  objects.resize(1); objects[0].mesh = Triangle(); return true;
}
bool LoadGdtfGeometryTree(const std::string &, GdtfGeometryTree &tree,
                          const std::string &, std::string *) {
  GdtfNode3D node;
  node.stableName = "Axis"; node.type = GdtfNodeType::Axis;
  tree.nodes = {node}; return true;
}
namespace ProjectUtils {
std::filesystem::path GetBaseLibraryPath(const std::string &) { return {}; }
std::string GetWritableLibraryPath(const std::string &) { return {}; }
std::string GetDefaultLibraryPath(const std::string &) { return {}; }
}
