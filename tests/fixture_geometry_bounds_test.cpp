#include "tools/fixture_geometry_bounds.h"

#include <cassert>
#include <string>
#include <vector>

#include "../viewer3d/gdtfloader.h"
#include "matrixutils.h"

namespace {

bool legacyLoaderCalled = false;

// Creates one mesh object whose vertex establishes a distinct extent.
GdtfObject MakeObject(float x, bool isLens) {
  GdtfObject object;
  object.transform = MatrixUtils::Identity();
  object.mesh.vertices = {0.0f, 0.0f, 0.0f, x, 1.0f, 1.0f};
  object.isLens = isLens;
  return object;
}

} // namespace

// Rejects accidental use of the legacy default-mode loader.
bool LoadGdtf(const std::string &, std::vector<GdtfObject> &, std::string *) {
  legacyLoaderCalled = true;
  return false;
}

// Supplies distinguishable geometry for each requested mode.
bool LoadGdtf(const std::string &, std::vector<GdtfObject> &objects,
              const std::string &modeName, std::string *) {
  objects = {MakeObject(modeName == "Wide" ? 20.0f : 5.0f, false),
             MakeObject(modeName == "Wide" ? 40.0f : 10.0f, true)};
  return true;
}

// Verifies exact mode selection and inclusion of rendered lens geometry.
int main() {
  tools::FixtureGeometryBounds narrow;
  tools::FixtureGeometryBounds wide;
  std::string error;
  assert(tools::ComputeFixtureGeometryBoundsMm("fixture.gdtf", "Narrow", narrow,
                                               error));
  assert(tools::ComputeFixtureGeometryBoundsMm("fixture.gdtf", "Wide", wide,
                                               error));
  assert(!legacyLoaderCalled);
  assert(narrow.valid && narrow.max[0] == 10.0f);
  assert(wide.valid && wide.max[0] == 40.0f);
  return 0;
}
