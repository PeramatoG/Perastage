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
#include "meshprimitives.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace {
constexpr float kPi = 3.14159265358979323846f;

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

void AddVertex(Mesh& mesh, float x, float y, float z)
{
    mesh.vertices.push_back(x);
    mesh.vertices.push_back(y);
    mesh.vertices.push_back(z);
}
}

Mesh BuildCubeMesh(float sizeX, float sizeY, float sizeZ)
{
    Mesh mesh;
    const float hx = sizeX * 0.5f;
    const float hy = sizeY * 0.5f;
    const float hz = sizeZ * 0.5f;

    mesh.vertices = {
        -hx, -hy, -hz,
         hx, -hy, -hz,
         hx,  hy, -hz,
        -hx,  hy, -hz,
        -hx, -hy,  hz,
         hx, -hy,  hz,
         hx,  hy,  hz,
        -hx,  hy,  hz,
    };

    mesh.indices = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        1, 5, 6, 1, 6, 2,
        2, 6, 7, 2, 7, 3,
        3, 7, 4, 3, 4, 0,
    };

    ComputeNormals(mesh);
    return mesh;
}

Mesh BuildCylinderMesh(float radius, float height, int segments)
{
    return BuildCylinderMesh(radius, radius, height, segments);
}

Mesh BuildCylinderMesh(float topRadius, float bottomRadius, float height, int segments)
{
    Mesh mesh;
    segments = std::max(segments, 3);
    topRadius = std::max(topRadius, 0.0f);
    bottomRadius = std::max(bottomRadius, 0.0f);

    const float halfH = height * 0.5f;
    const uint32_t topCenter = 0;
    const uint32_t bottomCenter = 1;
    AddVertex(mesh, 0.0f, 0.0f, halfH);
    AddVertex(mesh, 0.0f, 0.0f, -halfH);

    for (int i = 0; i < segments; ++i) {
        float a = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        float cosA = std::cos(a);
        float sinA = std::sin(a);
        AddVertex(mesh, cosA * topRadius, sinA * topRadius, halfH);
        AddVertex(mesh, cosA * bottomRadius, sinA * bottomRadius, -halfH);
    }

    for (int i = 0; i < segments; ++i) {
        uint32_t top0 = static_cast<uint32_t>(2 + i * 2);
        uint32_t bot0 = static_cast<uint32_t>(top0 + 1);
        uint32_t top1 = static_cast<uint32_t>(2 + ((i + 1) % segments) * 2);
        uint32_t bot1 = static_cast<uint32_t>(top1 + 1);

        mesh.indices.push_back(topCenter);
        mesh.indices.push_back(top0);
        mesh.indices.push_back(top1);

        mesh.indices.push_back(bottomCenter);
        mesh.indices.push_back(bot1);
        mesh.indices.push_back(bot0);

        mesh.indices.push_back(top0);
        mesh.indices.push_back(bot0);
        mesh.indices.push_back(top1);

        mesh.indices.push_back(top1);
        mesh.indices.push_back(bot0);
        mesh.indices.push_back(bot1);
    }

    ComputeNormals(mesh);
    return mesh;
}

Mesh BuildSphereMesh(float radius, int rings, int segments)
{
    Mesh mesh;
    rings = std::max(rings, 3);
    segments = std::max(segments, 3);

    for (int r = 0; r <= rings; ++r) {
        float v = static_cast<float>(r) / static_cast<float>(rings);
        float phi = v * kPi;
        float z = std::cos(phi) * radius;
        float rr = std::sin(phi) * radius;
        for (int s = 0; s <= segments; ++s) {
            float u = static_cast<float>(s) / static_cast<float>(segments);
            float theta = u * 2.0f * kPi;
            AddVertex(mesh, std::cos(theta) * rr, std::sin(theta) * rr, z);
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            uint32_t i0 = static_cast<uint32_t>(r * (segments + 1) + s);
            uint32_t i1 = static_cast<uint32_t>(i0 + segments + 1);
            uint32_t i2 = static_cast<uint32_t>(i0 + 1);
            uint32_t i3 = static_cast<uint32_t>(i1 + 1);

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i3);
        }
    }

    ComputeNormals(mesh);
    return mesh;
}

bool BuildPrimitiveMesh(const std::string& primitiveType, Mesh& outMesh)
{
    const std::string type = ToLower(primitiveType);
    auto parseFloatParam = [&type](std::string_view key, float fallback) {
        const std::string token = std::string(key) + "=";
        const size_t pos = type.find(token);
        if (pos == std::string::npos)
            return fallback;
        const size_t valueStart = pos + token.size();
        size_t valueEnd = type.find_first_of(";,&", valueStart);
        if (valueEnd == std::string::npos)
            valueEnd = type.size();
        const std::string value = type.substr(valueStart, valueEnd - valueStart);
        if (value.empty())
            return fallback;
        try {
            return std::stof(value);
        } catch (...) {
            return fallback;
        }
    };

    if (type == "cube" || type == "base" || type == "base1_1" ||
        type == "conventional" || type == "conventional1_1") {
        outMesh = BuildCubeMesh(1000.0f, 1000.0f, 1000.0f);
        return true;
    }

    if (type == "cylinder" || type == "yoke" || type == "scanner" ||
        type == "scanner1_1" || type == "pigtail") {
        outMesh = BuildCylinderMesh(500.0f, 1000.0f);
        return true;
    }

    if (type.rfind("cylinder;", 0) == 0 || type.rfind("cylinder,", 0) == 0 ||
        type.rfind("cylinder&", 0) == 0 || type.rfind("cylinder?", 0) == 0) {
        const float topRadiusMm = std::max(parseFloatParam("top", 500.0f), 0.0f);
        const float bottomRadiusMm = std::max(parseFloatParam("bottom", 500.0f), 0.0f);
        const float heightMm = std::max(parseFloatParam("height", 1000.0f), 1.0f);
        outMesh = BuildCylinderMesh(topRadiusMm, bottomRadiusMm, heightMm);
        return true;
    }

    if (type == "sphere" || type == "head") {
        outMesh = BuildSphereMesh(500.0f);
        return true;
    }

    return false;
}
