#include "tools/fixture_geometry_bounds.h"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

#include "../viewer3d/gdtfloader.h"
#include "matrixutils.h"

namespace {

bool legacyLoaderCalled = false;

// Creates translated and rotated geometry in the loader's mixed unit domain.
GdtfObject MakeObject(float x, bool isLens) {
  GdtfObject object;
  object.transform = MatrixUtils::Identity();
  object.transform.u = {0.0f, 1.0f, 0.0f};
  object.transform.v = {-1.0f, 0.0f, 0.0f};
  object.transform.o = {1.0f, -2.0f, 0.5f};
  object.mesh.vertices = {-x, -20.0f, -10.0f, x, 20.0f, 10.0f};
  object.isLens = isLens;
  return object;
}

// Reports whether two bounds coordinates agree within loader precision.
bool Near(float left, float right) { return std::fabs(left - right) < 0.001f; }

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
  assert(narrow.valid);
  assert(Near(narrow.min[0], 980.0f));
  assert(Near(narrow.max[0], 1020.0f));
  assert(Near(narrow.min[1], -2010.0f));
  assert(Near(narrow.max[1], -1990.0f));
  assert(Near(narrow.min[2], 490.0f));
  assert(Near(narrow.max[2], 510.0f));
  assert(wide.valid);
  assert(Near(wide.min[1], -2040.0f));
  assert(Near(wide.max[1], -1960.0f));
  return 0;
}
