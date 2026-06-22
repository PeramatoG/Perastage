#include "pick_mesh_validation.h"

#include <cassert>

// Verifies that valid picking triangles pass CPU-side bounds validation.
void ValidTrianglePasses() {
  Mesh mesh;
  mesh.vertices = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  mesh.indices = {0, 1, 2};

  PickMeshValidationStats stats;
  assert(IsPickTriangleIndexRangeValid(mesh, 0, &stats));
  assert(stats.invalidTriangleCount == 0);
  assert(stats.invalidIndexCount == 0);
}

// Verifies that invalid picking triangles are detected before vertex reads.
void InvalidTriangleIsSkipped() {
  Mesh mesh;
  mesh.vertices = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  mesh.indices = {0, 9, 2, 0, 1, 2};

  PickMeshValidationStats stats = ValidatePickMeshIndices(mesh);
  assert(!IsPickTriangleIndexRangeValid(mesh, 0));
  assert(IsPickTriangleIndexRangeValid(mesh, 3));
  assert(stats.vertexCount == 3);
  assert(stats.indexCount == 6);
  assert(stats.triangleCount == 2);
  assert(stats.invalidTriangleCount == 1);
  assert(stats.invalidIndexCount == 1);
}

// Runs picking mesh validation regression checks.
int main() {
  ValidTrianglePasses();
  InvalidTriangleIsSkipped();
  return 0;
}
