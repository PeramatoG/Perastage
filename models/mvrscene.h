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
#include <optional>
#include <vector>
#include <memory>
#include "fixture.h"
#include "truss.h"
#include "sceneobject.h"
#include "groupobject.h"
#include "support.h"
#include "layer.h"

struct SymdefGeometry {
    std::string file;
    Matrix transform;
};

// Canonical direct-geometry Symdef. Nested Symbol graphs remain protected.
struct SymdefDefinition {
    std::optional<std::string> name;
    std::vector<SymdefGeometry> geometries;
};

// Stores one validated foreign root UserData/Data subtree for opaque passthrough.
struct MvrOpaqueUserDataBlock {
    std::string provider;
    std::string version;
    std::string xml;

    bool operator==(const MvrOpaqueUserDataBlock& other) const = default;
};

enum class MvrPreservationIssueKind { StandardData, ResourceFailure };

// Durable evidence of source information the current serializer cannot retain.
// Evidence is diagnostic data, never an XML fragment to insert into output.
struct MvrPreservationIssue {
    std::string node;
    std::string objectUuid;
    std::string sourceLocation;
    std::string originalInformation;
    std::string reason;
    std::string recommendedAction;
    // ResourceFailure stores the unavailable resource name in originalInformation.
    MvrPreservationIssueKind kind = MvrPreservationIssueKind::StandardData;
    bool operator==(const MvrPreservationIssue&) const = default;
};

// Identity is the registry key; names are presentation and never merge keys.
struct MvrClass {
    std::optional<std::string> name;
    bool operator==(const MvrClass &) const = default;
};

// Represents the full scene structure from an MVR file.
class MvrScene {
public:
    void Clear();

    // Base directory where the MVR archive was extracted. All relative
    // resource paths (e.g. 3D models) are resolved against this path.
    std::string basePath;
    std::vector<std::shared_ptr<void>> runtimeResourceLeases;
    std::vector<MvrOpaqueUserDataBlock> opaqueUserDataBlocks;
    std::vector<MvrPreservationIssue> preservationIssues;

    std::unordered_map<std::string, Fixture> fixtures;
    std::unordered_map<std::string, Truss> trusses;
    std::unordered_map<std::string, Support> supports;
    std::unordered_map<std::string, SceneObject> sceneObjects;
    std::unordered_map<std::string, GroupObject> groupObjects;
    std::unordered_map<std::string, Layer> layers;
    // Lookup tables for additional references
    std::unordered_map<std::string, MvrClass> classes; // Persistent, scene-wide definitions.
    std::unordered_map<std::string, std::string> positions;   // uuid -> name
    std::unordered_map<std::string, SymdefDefinition> symdefs;

    std::string provider;
    std::string providerVersion;
    int versionMajor = 1;
    int versionMinor = 6;
};
