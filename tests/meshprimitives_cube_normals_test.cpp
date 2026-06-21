#include "meshprimitives.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {

struct ExpectedFaceNormal {
    size_t triangleOffset;
    std::array<float, 3> normal;
};

// Computes the normalized geometric normal for an indexed triangle.
std::array<float, 3> TriangleNormal(const Mesh& mesh, size_t triangleOffset)
{
    const uint32_t i0 = mesh.indices[triangleOffset];
    const uint32_t i1 = mesh.indices[triangleOffset + 1];
    const uint32_t i2 = mesh.indices[triangleOffset + 2];

    const float v0x = mesh.vertices[i0 * 3];
    const float v0y = mesh.vertices[i0 * 3 + 1];
    const float v0z = mesh.vertices[i0 * 3 + 2];
    const float v1x = mesh.vertices[i1 * 3];
    const float v1y = mesh.vertices[i1 * 3 + 1];
    const float v1z = mesh.vertices[i1 * 3 + 2];
    const float v2x = mesh.vertices[i2 * 3];
    const float v2y = mesh.vertices[i2 * 3 + 1];
    const float v2z = mesh.vertices[i2 * 3 + 2];

    float nx = (v1y - v0y) * (v2z - v0z) - (v1z - v0z) * (v2y - v0y);
    float ny = (v1z - v0z) * (v2x - v0x) - (v1x - v0x) * (v2z - v0z);
    float nz = (v1x - v0x) * (v2y - v0y) - (v1y - v0y) * (v2x - v0x);

    const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
    assert(length > 0.0f);
    return {nx / length, ny / length, nz / length};
}

// Verifies that two normal vectors match within a small tolerance.
void AssertNormal(const std::array<float, 3>& actual, const std::array<float, 3>& expected)
{
    constexpr float kTolerance = 0.0001f;
    assert(std::fabs(actual[0] - expected[0]) < kTolerance);
    assert(std::fabs(actual[1] - expected[1]) < kTolerance);
    assert(std::fabs(actual[2] - expected[2]) < kTolerance);
}

}

// Verifies cube primitive triangle winding generates outward face normals.
int main()
{
    const Mesh mesh = BuildCubeMesh(2.0f, 4.0f, 6.0f);
    assert(mesh.indices.size() == 36);

    const ExpectedFaceNormal expectedFaces[] = {
        {0, {0.0f, 0.0f, -1.0f}},
        {6, {0.0f, 0.0f, 1.0f}},
        {12, {0.0f, -1.0f, 0.0f}},
        {24, {0.0f, 1.0f, 0.0f}},
        {30, {-1.0f, 0.0f, 0.0f}},
        {18, {1.0f, 0.0f, 0.0f}},
    };

    for (const ExpectedFaceNormal& face : expectedFaces) {
        AssertNormal(TriangleNormal(mesh, face.triangleOffset), face.normal);
        AssertNormal(TriangleNormal(mesh, face.triangleOffset + 3), face.normal);
    }

    return 0;
}
