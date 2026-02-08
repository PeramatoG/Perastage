/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>

struct Mesh {
    std::vector<float> vertices; // x,y,z order in mm
    std::vector<unsigned short> indices; // 3 indices per triangle
    std::vector<float> normals;  // optional per-vertex normals

    // OpenGL resources for VBO/VAO based rendering.
    uint32_t vao = 0;
    uint32_t vboVertices = 0;
    uint32_t vboNormals = 0;
    uint32_t eboTriangles = 0;
    uint32_t eboLines = 0;
    int triangleIndexCount = 0;
    int lineIndexCount = 0;
    bool buffersReady = false;
    // True once we have evaluated/fixed triangle winding for compatibility
    // with assets coming from heterogeneous DCC/export pipelines.
    bool windingChecked = false;
    // Some imported assets contain inconsistent per-face winding. Rendering
    // them double-sided avoids accidental face dropouts caused by backface
    // culling while keeping culling enabled for well-formed meshes.
    bool preferDoubleSided = false;
};

// Returns true when the mesh looks like it has mixed winding, which is a
// strong indicator that backface culling can remove visible triangles.
inline bool NeedsDoubleSidedRendering(const Mesh& mesh)
{
    const size_t vcount = mesh.vertices.size() / 3;
    if (vcount < 3 || mesh.indices.size() < 6)
        return false;

    float cx = 0.0f;
    float cy = 0.0f;
    float cz = 0.0f;
    for (size_t i = 0; i < vcount; ++i) {
        cx += mesh.vertices[i * 3];
        cy += mesh.vertices[i * 3 + 1];
        cz += mesh.vertices[i * 3 + 2];
    }
    const float invCount = 1.0f / static_cast<float>(vcount);
    cx *= invCount;
    cy *= invCount;
    cz *= invCount;

    size_t positiveFaces = 0;
    size_t negativeFaces = 0;
    size_t validFaces = 0;

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const unsigned short i0 = mesh.indices[i];
        const unsigned short i1 = mesh.indices[i + 1];
        const unsigned short i2 = mesh.indices[i + 2];

        const float v0x = mesh.vertices[i0 * 3];
        const float v0y = mesh.vertices[i0 * 3 + 1];
        const float v0z = mesh.vertices[i0 * 3 + 2];
        const float v1x = mesh.vertices[i1 * 3];
        const float v1y = mesh.vertices[i1 * 3 + 1];
        const float v1z = mesh.vertices[i1 * 3 + 2];
        const float v2x = mesh.vertices[i2 * 3];
        const float v2y = mesh.vertices[i2 * 3 + 1];
        const float v2z = mesh.vertices[i2 * 3 + 2];

        const float ux = v1x - v0x;
        const float uy = v1y - v0y;
        const float uz = v1z - v0z;
        const float vx = v2x - v0x;
        const float vy = v2y - v0y;
        const float vz = v2z - v0z;

        const float nx = uy * vz - uz * vy;
        const float ny = uz * vx - ux * vz;
        const float nz = ux * vy - uy * vx;
        const double area = std::sqrt(static_cast<double>(nx) * nx +
                                      static_cast<double>(ny) * ny +
                                      static_cast<double>(nz) * nz);
        if (area <= 1e-9)
            continue;

        const float tx = (v0x + v1x + v2x) / 3.0f;
        const float ty = (v0y + v1y + v2y) / 3.0f;
        const float tz = (v0z + v1z + v2z) / 3.0f;
        const float toCx = tx - cx;
        const float toCy = ty - cy;
        const float toCz = tz - cz;

        const double sign = static_cast<double>(nx) * toCx +
                            static_cast<double>(ny) * toCy +
                            static_cast<double>(nz) * toCz;
        if (sign > 0.0)
            ++positiveFaces;
        else if (sign < 0.0)
            ++negativeFaces;
        ++validFaces;
    }

    if (validFaces < 4)
        return false;

    const double dominant =
        static_cast<double>(std::max(positiveFaces, negativeFaces));
    const double consistency = dominant / static_cast<double>(validFaces);
    return consistency < 0.80;
}

// Attempts to detect meshes whose triangle winding is globally inverted and
// flips their index order when needed. This runs once per mesh at upload time,
// so it has no per-frame rendering overhead.
inline void EnsureOutwardWinding(Mesh& mesh)
{
    if (mesh.windingChecked)
        return;

    mesh.windingChecked = true;

    const size_t vcount = mesh.vertices.size() / 3;
    if (vcount < 3 || mesh.indices.size() < 3)
        return;

    float cx = 0.0f;
    float cy = 0.0f;
    float cz = 0.0f;
    for (size_t i = 0; i < vcount; ++i) {
        cx += mesh.vertices[i * 3];
        cy += mesh.vertices[i * 3 + 1];
        cz += mesh.vertices[i * 3 + 2];
    }
    const float invCount = 1.0f / static_cast<float>(vcount);
    cx *= invCount;
    cy *= invCount;
    cz *= invCount;

    double orientationScore = 0.0;
    double totalArea = 0.0;

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const unsigned short i0 = mesh.indices[i];
        const unsigned short i1 = mesh.indices[i + 1];
        const unsigned short i2 = mesh.indices[i + 2];

        const float v0x = mesh.vertices[i0 * 3];
        const float v0y = mesh.vertices[i0 * 3 + 1];
        const float v0z = mesh.vertices[i0 * 3 + 2];
        const float v1x = mesh.vertices[i1 * 3];
        const float v1y = mesh.vertices[i1 * 3 + 1];
        const float v1z = mesh.vertices[i1 * 3 + 2];
        const float v2x = mesh.vertices[i2 * 3];
        const float v2y = mesh.vertices[i2 * 3 + 1];
        const float v2z = mesh.vertices[i2 * 3 + 2];

        const float ux = v1x - v0x;
        const float uy = v1y - v0y;
        const float uz = v1z - v0z;
        const float vx = v2x - v0x;
        const float vy = v2y - v0y;
        const float vz = v2z - v0z;

        const float nx = uy * vz - uz * vy;
        const float ny = uz * vx - ux * vz;
        const float nz = ux * vy - uy * vx;

        const double area = std::sqrt(static_cast<double>(nx) * nx +
                                      static_cast<double>(ny) * ny +
                                      static_cast<double>(nz) * nz);
        if (area <= 1e-9)
            continue;

        const float tx = (v0x + v1x + v2x) / 3.0f;
        const float ty = (v0y + v1y + v2y) / 3.0f;
        const float tz = (v0z + v1z + v2z) / 3.0f;
        const float toCx = tx - cx;
        const float toCy = ty - cy;
        const float toCz = tz - cz;

        orientationScore += static_cast<double>(nx) * toCx +
                            static_cast<double>(ny) * toCy +
                            static_cast<double>(nz) * toCz;
        totalArea += area;
    }

    // Only act when the signal is clear enough; for flat/open meshes the
    // score can be near zero and should be left untouched.
    if (totalArea <= 1e-9 || std::abs(orientationScore) <= totalArea * 1e-4)
        return;

    if (orientationScore < 0.0) {
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
            std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);

        if (!mesh.normals.empty()) {
            for (size_t i = 0; i + 2 < mesh.normals.size(); i += 3) {
                mesh.normals[i] = -mesh.normals[i];
                mesh.normals[i + 1] = -mesh.normals[i + 1];
                mesh.normals[i + 2] = -mesh.normals[i + 2];
            }
        }
    }
}

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
