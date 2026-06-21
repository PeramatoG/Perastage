#include "mesh.h"

#include <cassert>
#include <cmath>

// Returns whether two floating-point values are nearly equal.
static bool NearlyEqual(float a, float b)
{
    return std::fabs(a - b) < 1e-6f;
}

// Verifies flat normals follow valid imported vertex-normal orientation.
static void TestFlatNormalsAlignWithVertexNormals()
{
    Mesh mesh;
    mesh.vertices = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    mesh.indices = {0, 2, 1};
    mesh.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };

    BuildFlatShadingBuffers(mesh);

    assert(mesh.flatNormals.size() == 9);
    for (size_t i = 0; i + 2 < mesh.flatNormals.size(); i += 3) {
        assert(NearlyEqual(mesh.flatNormals[i], 0.0f));
        assert(NearlyEqual(mesh.flatNormals[i + 1], 0.0f));
        assert(NearlyEqual(mesh.flatNormals[i + 2], 1.0f));
    }
}

// Verifies degenerate triangles keep the documented fallback normal.
static void TestDegenerateTriangleUsesFallbackNormal()
{
    Mesh mesh;
    mesh.vertices = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
    };
    mesh.indices = {0, 1, 2};
    mesh.normals = {
        0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, -1.0f,
    };

    BuildFlatShadingBuffers(mesh);

    assert(mesh.flatNormals.size() == 9);
    for (size_t i = 0; i + 2 < mesh.flatNormals.size(); i += 3) {
        assert(NearlyEqual(mesh.flatNormals[i], 0.0f));
        assert(NearlyEqual(mesh.flatNormals[i + 1], 0.0f));
        assert(NearlyEqual(mesh.flatNormals[i + 2], 1.0f));
    }
}

// Runs mesh flat-shading buffer regression checks.
int main()
{
    TestFlatNormalsAlignWithVertexNormals();
    TestDegenerateTriangleUsesFallbackNormal();
    return 0;
}
