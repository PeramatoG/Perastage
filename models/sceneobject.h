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

#include <vector>
#include <string>
#include "types.h"

struct GeometryInstance {
    std::string modelFile;
    Matrix localTransform = Matrix{};
};

// Represents a generic SceneObject parsed from MVR
struct SceneObject {
    std::string uuid;
    std::string name;
    std::string layer;
    std::string modelFile; // Referenced 3D model file (Geometry3D or Symdef)
    std::vector<GeometryInstance> geometries;
    Matrix transform;

    std::string GetPrimaryModel() const {
        if (!geometries.empty())
            return geometries.front().modelFile;
        return modelFile;
    }
};
