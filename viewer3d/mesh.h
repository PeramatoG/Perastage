#pragma once
#include <vector>
#include <cmath>

struct Mesh {
    std::vector<float> vertices; // x,y,z order in mm
    std::vector<unsigned short> indices; // 3 indices per triangle
    std::vector<float> normals;  // optional per-vertex normals
};

// Compute smooth per-vertex normals based on the indexed triangles. The
// resulting normal vector array will have the same vertex count as the
// mesh and will be stored in Mesh::normals.
inline void ComputeNormals(Mesh& mesh)
{
    size_t vcount = mesh.vertices.size() / 3;
    mesh.normals.assign(vcount * 3, 0.0f);

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        unsigned short i0 = mesh.indices[i];
        unsigned short i1 = mesh.indices[i + 1];
        unsigned short i2 = mesh.indices[i + 2];

        float v0x = mesh.vertices[i0 * 3];
        float v0y = mesh.vertices[i0 * 3 + 1];
        float v0z = mesh.vertices[i0 * 3 + 2];

        float v1x = mesh.vertices[i1 * 3];
        float v1y = mesh.vertices[i1 * 3 + 1];
        float v1z = mesh.vertices[i1 * 3 + 2];

        float v2x = mesh.vertices[i2 * 3];
        float v2y = mesh.vertices[i2 * 3 + 1];
        float v2z = mesh.vertices[i2 * 3 + 2];

        float ux = v1x - v0x;
        float uy = v1y - v0y;
        float uz = v1z - v0z;

        float vx = v2x - v0x;
        float vy = v2y - v0y;
        float vz = v2z - v0z;

        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;

        mesh.normals[i0 * 3]     += nx;
        mesh.normals[i0 * 3 + 1] += ny;
        mesh.normals[i0 * 3 + 2] += nz;
        mesh.normals[i1 * 3]     += nx;
        mesh.normals[i1 * 3 + 1] += ny;
        mesh.normals[i1 * 3 + 2] += nz;
        mesh.normals[i2 * 3]     += nx;
        mesh.normals[i2 * 3 + 1] += ny;
        mesh.normals[i2 * 3 + 2] += nz;
    }

    for (size_t i = 0; i < vcount; ++i) {
        float nx = mesh.normals[i * 3];
        float ny = mesh.normals[i * 3 + 1];
        float nz = mesh.normals[i * 3 + 2];
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0f) {
            mesh.normals[i * 3]     = nx / len;
            mesh.normals[i * 3 + 1] = ny / len;
            mesh.normals[i * 3 + 2] = nz / len;
        }
    }
}
