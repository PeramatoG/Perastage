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
#include <vector>

#include "mesh.h"
#include "types.h"

struct GdtfObject {
    Mesh mesh;
    Matrix transform; // local transform relative to fixture
    bool isLens = false;
};

enum class GdtfNodeType {
    Geometry,
    Axis,
    Emitter
};

struct GdtfNode3D {
    std::string stableName;
    GdtfNodeType type = GdtfNodeType::Geometry;
    int parentIndex = -1;
    Matrix localTransform = Matrix{};
    Matrix worldTransform = Matrix{};
    bool isLens = false;
    bool hasMesh = false;
    Mesh mesh;
};

struct GdtfGeometryTree {
    std::vector<GdtfNode3D> nodes;
    std::vector<int> axisNodeIndices;
    std::vector<int> emitterNodeIndices;
};
