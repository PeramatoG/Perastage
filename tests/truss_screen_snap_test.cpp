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

  const auto depthIndependent =
      truss_screen_snap::ClosestPointOnProjectedPolyline(
          Snapshot(1.0), {{-500.0f, 0.0f, 800.0f}, {500.0f, 0.0f, 800.0f}},
          {0.0f, 20.0f, -800.0f});
  assert(depthIndependent);
  assert(std::fabs(depthIndependent->screenDistanceLogicalPx - 10.0) < 0.001);
  assert(std::fabs(depthIndependent->worldPointMm[2] - 800.0f) < 0.001f);
  assert(std::fabs(depthIndependent->pathParameter - 0.5f) < 0.001f);
  assert(depthIndependent->worldDistanceMm > 1500.0);

  const auto outsideAperture =
      truss_screen_snap::ClosestPointOnProjectedPolyline(
          Snapshot(1.0), {{-500.0f, 0.0f, 800.0f}, {500.0f, 0.0f, 800.0f}},
          {0.0f, 40.0f, -800.0f});
  assert(outsideAperture);
  assert(outsideAperture->screenDistanceLogicalPx >
         truss_screen_snap::kDefaultFixturePathScreenSnapApertureLogicalPx);

  auto perspectivePolyline = Snapshot(1.0);
  perspectivePolyline.projection = {1, 0, 0, 0, 0, 1, 0, 0,
                                    0, 0, 1, 1, 0, 0, 0, 0};
  const auto perspectiveClosest =
      truss_screen_snap::ClosestPointOnProjectedPolyline(
          perspectivePolyline,
          {{-200.0f, 0.0f, 400.0f}, {800.0f, 0.0f, 800.0f}},
          {0.0f, 20.0f, 500.0f});
  assert(perspectiveClosest);
  assert(std::fabs(perspectiveClosest->worldPointMm[0]) < 0.01f);
  assert(std::fabs(perspectiveClosest->worldPointMm[2] - 480.0f) < 0.01f);
  assert(std::fabs(perspectiveClosest->pathParameter - 0.2f) < 0.001f);
  return 0;
}
