#pragma once

#include "canvas2d.h"
#include "viewer3d_types.h"
#include "symbolcache.h"
#include "types.h"

#include <array>
#include <string>

std::string NormalizeModelKey(const std::string &p);
std::string ResolveCacheKey(const std::string &pathRef);

SymbolBounds ComputeSymbolBounds(const CommandBuffer &buffer);
void MatrixToArray(const Matrix &m, float out[16]);
std::array<float, 3> TransformPoint(const Matrix &m,
                                    const std::array<float, 3> &p);
Transform2D BuildInstanceTransform2D(const Matrix &m, Viewer2DView view);

enum class FixtureSymbolPlane {
  Top,
  Front,
  Side,
};

struct FixtureSymbolProjection {
  FixtureSymbolPlane plane = FixtureSymbolPlane::Top;
  SymbolViewKind symbolView = SymbolViewKind::Top;
  Transform2D instanceTransform{};
  float projectedArea = 0.0f;
  bool valid = false;
};

FixtureSymbolProjection ResolveFixtureSymbolProjection(
    const Matrix &fixtureTransform, Viewer2DView targetView,
    bool forceBottomViewForTopFixtures);
