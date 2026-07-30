#include "truss_screen_snap.h"

#include <cassert>
#include <cmath>

namespace {

// Returns an identity OpenGL matrix.
std::array<double, 16> Identity() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

// Builds an orthographic projection snapshot at a requested content scale.
truss_screen_snap::ProjectionSnapshot Snapshot(double contentScale) {
  truss_screen_snap::ProjectionSnapshot snapshot;
  snapshot.modelView = Identity();
  snapshot.projection = Identity();
  snapshot.viewport = {0, 0, static_cast<int>(1000 * contentScale),
                       static_cast<int>(1000 * contentScale)};
  snapshot.contentScale = contentScale;
  return snapshot;
}

} // namespace

// Verifies pure projection, clipping, and logical-pixel DPI behavior.
int main() {
  for (double scale : {1.0, 1.5, 2.0}) {
    const auto snapshot = Snapshot(scale);
    const auto origin = truss_screen_snap::Project(snapshot, {0, 0, 0});
    const auto near = truss_screen_snap::Project(snapshot, {30, 0, 0});
    const auto far = truss_screen_snap::Project(snapshot, {34, 0, 0});
    assert(origin && near && far);
    assert(std::fabs(near->logicalX - origin->logicalX - 15.0) < 0.001);
    assert(std::fabs(far->logicalX - origin->logicalX - 17.0) < 0.001);
  }

  auto perspective = Snapshot(1.0);
  perspective.projection = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0};
  assert(truss_screen_snap::Project(perspective, {100, 0, 500}));
  assert(!truss_screen_snap::Project(perspective, {100, 0, -500}));
  assert(!truss_screen_snap::Project(Snapshot(1.0), {2000, 0, 0}));
  return 0;
}
