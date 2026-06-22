#include "pick_mesh_validation.h"

namespace {

// Returns true when a vertex index maps to three available position floats.
bool IsVertexIndexValid(uint32_t index, size_t vertexCount) {
  return static_cast<size_t>(index) < vertexCount;
}

} // namespace

// Validates whether a triangle can safely read CPU-side mesh vertex data.
bool IsPickTriangleIndexRangeValid(const Mesh &mesh, size_t firstIndex,
                                   PickMeshValidationStats *stats) {
  const size_t vertexCount = mesh.vertices.size() / 3;
  if (stats) {
    stats->vertexCount = vertexCount;
    stats->indexCount = mesh.indices.size();
  }
  if (firstIndex + 2 >= mesh.indices.size())
    return false;

  bool valid = true;
  for (size_t offset = 0; offset < 3; ++offset) {
    if (!IsVertexIndexValid(mesh.indices[firstIndex + offset], vertexCount)) {
      valid = false;
      if (stats)
        ++stats->invalidIndexCount;
    }
  }
  if (stats) {
    ++stats->triangleCount;
    if (!valid)
      ++stats->invalidTriangleCount;
  }
  return valid;
}

// Counts invalid picking triangles without reading out-of-range vertex data.
PickMeshValidationStats ValidatePickMeshIndices(const Mesh &mesh) {
  PickMeshValidationStats stats;
  stats.vertexCount = mesh.vertices.size() / 3;
  stats.indexCount = mesh.indices.size();
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    IsPickTriangleIndexRangeValid(mesh, i, &stats);
  return stats;
}
