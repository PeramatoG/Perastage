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

#include <unordered_map>
#include <string>
#include <vector>
#include "fixture.h"
#include "truss.h"
#include "sceneobject.h"
#include "support.h"
#include "layer.h"

struct SymdefGeometry {
    std::string file;
    std::string geometryType;
    Matrix transform;
};

// Represents the full scene structure from an MVR file
class MvrScene {
public:
    void Clear();

    // Base directory where the MVR archive was extracted. All relative
    // resource paths (e.g. 3D models) are resolved against this path.
    std::string basePath;

    std::unordered_map<std::string, Fixture> fixtures;
    std::unordered_map<std::string, Truss> trusses;
    std::unordered_map<std::string, Support> supports;
    std::unordered_map<std::string, SceneObject> sceneObjects;
    std::unordered_map<std::string, Layer> layers;
    // Lookup tables for additional references
    std::unordered_map<std::string, std::string> positions;   // uuid -> name
    std::unordered_map<std::string, std::string> symdefFiles; // uuid -> geometry file
    std::unordered_map<std::string, std::string> symdefTypes; // uuid -> geometry type
    std::unordered_map<std::string, Matrix> symdefMatrices;    // uuid -> first geometry matrix
    std::unordered_map<std::string, std::vector<SymdefGeometry>> symdefGeometries;

    std::string provider;
    std::string providerVersion;
    int versionMajor = 1;
    int versionMinor = 6;
};
