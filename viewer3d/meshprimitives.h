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

#include <string>
#include "mesh.h"

// Builds a cube centered at origin with the given dimensions in millimeters.
Mesh BuildCubeMesh(float sizeX, float sizeY, float sizeZ);

// Builds a cylinder centered at origin, axis aligned on Z, dimensions in mm.
Mesh BuildCylinderMesh(float radius, float height, int segments = 24);

// Builds a UV sphere centered at origin, dimensions in mm.
Mesh BuildSphereMesh(float radius, int rings = 12, int segments = 24);

// Builds a mesh from a GDTF PrimitiveType string. Returns false when the
// primitive type is not representable.
bool BuildPrimitiveMesh(const std::string& primitiveType, Mesh& outMesh);
