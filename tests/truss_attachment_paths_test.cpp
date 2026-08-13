#include "matrixutils.h"
#include "truss_attachment_paths.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {

struct Geometry {
  std::vector<float> vertices;
  std::vector<std::uint32_t> indices;
};

// Appends a welded rectangular member to the test mesh.
void AddBox(Geometry &geometry, float x0, float x1, float y, float z,
            float halfThickness = 12.0f) {
  const std::uint32_t base = geometry.vertices.size() / 3;
  for (float x : {x0, x1})
    for (float dy : {-halfThickness, halfThickness})
      for (float dz : {-halfThickness, halfThickness})
        geometry.vertices.insert(geometry.vertices.end(), {x, y + dy, z + dz});
  const std::uint32_t faces[][6] = {{0, 1, 3, 0, 3, 2}, {4, 6, 7, 4, 7, 5},
                                    {0, 4, 5, 0, 5, 1}, {2, 3, 7, 2, 7, 6},
                                    {0, 2, 6, 0, 6, 4}, {1, 5, 7, 1, 7, 3}};
  for (const auto &face : faces)
    for (std::uint32_t index : face)
      geometry.indices.push_back(base + index);
}

// Builds one welded mesh with the requested persistent chord centers.
Geometry MakeTruss(const std::vector<std::array<float, 2>> &centers) {
  Geometry geometry;
  for (const auto &center : centers)
    AddBox(geometry, 0.0f, 3000.0f, center[0], center[1]);
  AddBox(geometry, 1100.0f, 1350.0f, 0.0f, 0.0f, 8.0f);
  return geometry;
}

// Reports a deterministic failed assertion.
bool Check(bool condition, const char *expression, int line) {
  if (!condition)
    std::cerr << "Failed at line " << line << ": " << expression << '\n';
  return condition;
}

#define CHECK(value)                                                           \
  do {                                                                         \
    if (!Check((value), #value, __LINE__))                                     \
      return 1;                                                                \
  } while (false)

} // namespace

// Exercises persistent chord detection, transforms, and conservative fallback.
int main() {
  using namespace truss_attachment_paths;
  const auto square = MakeTruss({{-150.0f, -150.0f},
                                 {-150.0f, 150.0f},
                                 {150.0f, -150.0f},
                                 {150.0f, 150.0f}});
  const auto squarePaths = AnalyzeMesh(square.vertices, square.indices);
  CHECK(squarePaths.size() == 4);
  CHECK(squarePaths.front().diagnostics.longitudinalCoverage >= 0.70f);

  const auto triangle =
      MakeTruss({{-170.0f, -120.0f}, {170.0f, -120.0f}, {0.0f, 180.0f}});
  CHECK(AnalyzeMesh(triangle.vertices, triangle.indices).size() == 3);

  const auto ladder = MakeTruss({{0.0f, -140.0f}, {0.0f, 140.0f}});
  CHECK(AnalyzeMesh(ladder.vertices, ladder.indices).size() == 2);

  Matrix transform = MatrixUtils::Identity();
  transform.u = {0.0f, 1.0f, 0.0f};
  transform.v = {-1.0f, 0.0f, 0.0f};
  transform.o = {100.0f, 200.0f, 300.0f};
  const Path world = TransformPath(squarePaths.front(), transform, "rotated");
  CHECK(world.ownerTrussUuid == "rotated");
  CHECK(std::fabs(world.worldPointsMm.back()[1] -
                  world.worldPointsMm.front()[1] - 3000.0f) < 0.01f);
  const auto closest =
      ClosestPointOnPath(world, {world.worldPointsMm.front()[0] + 40.0f,
                                 1700.0f, world.worldPointsMm.front()[2]});
  CHECK(closest.has_value());
  CHECK(closest->pathParameter > 0.4f && closest->pathParameter < 0.6f);

  MvrScene scene;
  Truss incomplete;
  incomplete.uuid = "incomplete";
  incomplete.transform = MatrixUtils::Identity();
  Resolver resolver;
  const auto fallback = resolver.Resolve(scene, incomplete);
  CHECK(fallback.paths.empty());
  CHECK(fallback.usedBoundsFallback);
  CHECK(resolver.GeometryParseCount() == 0);

  Path degenerate;
  degenerate.localPointsMm = {{2.0f, 3.0f, 4.0f}, {2.0f, 3.0f, 4.0f}};
  const auto degenerateClosest =
      ClosestPointOnPath(degenerate, {5.0f, 7.0f, 4.0f}, false);
  CHECK(degenerateClosest.has_value());
  CHECK(std::fabs(degenerateClosest->distanceMm - 5.0f) < 0.001f);
  CHECK(degenerateClosest->pathParameter == 0.0f);

  CHECK(AnalyzeMesh({0.0f, 0.0f, 0.0f}, {}).empty());
  CHECK(AnalyzeMesh({0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                     std::numeric_limits<float>::quiet_NaN(), 1.0f, 0.0f},
                    {0, 1, 2})
            .empty());
  return 0;
}
