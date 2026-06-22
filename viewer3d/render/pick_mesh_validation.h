#pragma once

#include "mesh.h"

#include <cstddef>
#include <cstdint>

struct PickMeshValidationStats {
  size_t vertexCount = 0;
  size_t indexCount = 0;
  size_t triangleCount = 0;
  size_t invalidTriangleCount = 0;
  size_t invalidIndexCount = 0;
};

// Validates whether a triangle can safely read CPU-side mesh vertex data.
bool IsPickTriangleIndexRangeValid(const Mesh &mesh, size_t firstIndex,
                                   PickMeshValidationStats *stats = nullptr);

// Counts invalid picking triangles without reading out-of-range vertex data.
PickMeshValidationStats ValidatePickMeshIndices(const Mesh &mesh);
