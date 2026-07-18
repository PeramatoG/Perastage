#include "opaque_pass_utils.h"

#include <cassert>
#include <cmath>
#include <iostream>

// Returns a matrix with caller-provided local basis vectors.
Matrix MakeMatrix(std::array<float, 3> u, std::array<float, 3> v,
                  std::array<float, 3> w) {
  Matrix m{};
  m.u = u;
  m.v = v;
  m.w = w;
  return m;
}

// Returns true when two floating-point values are approximately equal.
bool Near(float a, float b, float epsilon = 1.0e-5f) {
  return std::abs(a - b) <= epsilon;
}

// Returns the determinant of a 2D transform linear part.
float Determinant(const Transform2D &t) { return t.a * t.d - t.b * t.c; }

// Verifies the projection resolver for representative layout fixture cases.
int main() {
  Matrix identity{};
  auto top = ResolveFixtureSymbolProjection(identity, Viewer2DView::Top, false);
  assert(top.valid);
  assert(top.plane == FixtureSymbolPlane::Top);
  assert(top.symbolView == SymbolViewKind::Top);

  auto front = ResolveFixtureSymbolProjection(identity, Viewer2DView::Front, false);
  assert(front.valid);
  assert(front.plane == FixtureSymbolPlane::Front);
  assert(front.symbolView == SymbolViewKind::Front);

  auto side = ResolveFixtureSymbolProjection(identity, Viewer2DView::Side, false);
  assert(side.valid);
  assert(side.plane == FixtureSymbolPlane::Side);
  assert(side.symbolView == SymbolViewKind::Left);

  auto forced = ResolveFixtureSymbolProjection(identity, Viewer2DView::Top, true);
  assert(forced.plane == FixtureSymbolPlane::Top);
  assert(forced.symbolView == SymbolViewKind::Bottom);

  Matrix jdc = MakeMatrix({1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                          {0.0f, -1.0f, 0.0f});
  auto jdcTop = ResolveFixtureSymbolProjection(jdc, Viewer2DView::Top, false);
  assert(jdcTop.valid);
  assert(jdcTop.plane == FixtureSymbolPlane::Front);
  assert(jdcTop.symbolView == SymbolViewKind::Front);
  assert(Near(std::abs(Determinant(jdcTop.instanceTransform)), 1.0f));
  assert(Near(jdcTop.instanceTransform.b, 0.0f));
  assert(Near(jdcTop.instanceTransform.c, 0.0f));

  Matrix colorado = MakeMatrix({1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                               {0.0f, -1.0f, 0.0f});
  auto coloradoFront =
      ResolveFixtureSymbolProjection(colorado, Viewer2DView::Front, false);
  assert(coloradoFront.valid);
  assert(coloradoFront.plane == FixtureSymbolPlane::Top);
  assert(coloradoFront.symbolView == SymbolViewKind::Top);
  assert(Near(std::abs(Determinant(coloradoFront.instanceTransform)), 1.0f));
  assert(Near(coloradoFront.instanceTransform.d, 1.0f));

  Matrix sideEdge = MakeMatrix({0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                               {0.0f, 0.0f, 1.0f});
  auto sideRotated =
      ResolveFixtureSymbolProjection(sideEdge, Viewer2DView::Side, false);
  assert(sideRotated.valid);
  assert(sideRotated.plane == FixtureSymbolPlane::Front);

  Matrix rotated180 = MakeMatrix({-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
                                 {0.0f, 0.0f, 1.0f});
  auto top180 =
      ResolveFixtureSymbolProjection(rotated180, Viewer2DView::Top, false);
  assert(top180.valid);
  assert(top180.plane == FixtureSymbolPlane::Top);
  assert(Determinant(top180.instanceTransform) > 0.0f);
  assert(Near(top180.instanceTransform.a, -1.0f));
  assert(Near(top180.instanceTransform.d, -1.0f));

  Matrix arbitrary =
      MakeMatrix({0.7071067f, 0.5f, 0.5f},
                 {-0.7071067f, 0.5f, 0.5f},
                 {0.0f, -0.7071067f, 0.7071067f});
  auto arbitraryTop =
      ResolveFixtureSymbolProjection(arbitrary, Viewer2DView::Top, false);
  assert(arbitraryTop.valid);
  assert(std::isfinite(arbitraryTop.projectedArea));
  assert(std::isfinite(arbitraryTop.instanceTransform.a));
  assert(arbitraryTop.projectedArea > 0.0f);

  Matrix degenerate{};
  degenerate.u = {0.0f, 0.0f, 0.0f};
  degenerate.v = {0.0f, 0.0f, 0.0f};
  degenerate.w = {0.0f, 0.0f, 0.0f};
  auto degenerateProjection =
      ResolveFixtureSymbolProjection(degenerate, Viewer2DView::Top, false);
  assert(!degenerateProjection.valid);
  assert(degenerateProjection.plane == FixtureSymbolPlane::Top);

  auto frontForced = ResolveFixtureSymbolProjection(jdc, Viewer2DView::Top, true);
  assert(frontForced.plane == FixtureSymbolPlane::Front);
  assert(frontForced.symbolView == SymbolViewKind::Front);

  std::cout << "fixture_symbol_projection_test passed" << std::endl;
  return 0;
}
