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

#include "gdtf_geometry_types.h"

// Metadata for a GDTF model definition. Length/Width/Height correspond
// to the desired bounding box dimensions in meters as specified in the
// GDTF file. When any of these are zero the raw mesh size is used.
struct GdtfModelInfo {
    std::string file;
    std::string primitiveType;
    float length = 0.0f; // meters
    float width  = 0.0f; // meters
    float height = 0.0f; // meters
};

// Simple representation of a DMX channel/function pair
struct GdtfChannelInfo {
    int channel = 0;           // DMX channel number (coarse)
    std::string function;      // Associated function/attribute
    bool isVirtual = false;    // True when Offset is omitted/None in GDTF
};

// Loads the models defined in a GDTF file. Returns true on success.
bool LoadGdtf(const std::string& gdtfPath,
              std::vector<GdtfObject>& outObjects,
              std::string* outError = nullptr);

// Loads the models for a selected GDTF DMX mode while preserving the legacy API.
bool LoadGdtf(const std::string& gdtfPath,
              std::vector<GdtfObject>& outObjects,
              const std::string& modeName,
              std::string* outError = nullptr);

// Loads the geometry hierarchy defined by a GDTF file preserving each node's
// local transform and exposing stable node names for geometry, axis and
// emitter nodes.
bool LoadGdtfGeometryTree(const std::string& gdtfPath,
                          GdtfGeometryTree& outTree,
                          std::string* outError = nullptr);

// Loads the geometry hierarchy for a selected GDTF DMX mode.
bool LoadGdtfGeometryTree(const std::string& gdtfPath,
                          GdtfGeometryTree& outTree,
                          const std::string& modeName,
                          std::string* outError = nullptr);

// Returns the DMX footprint (highest occupied offset) of the given mode in a
// GDTF file for DMX break 1 style addressing. Returns -1 when the mode cannot
// be found or the file cannot be parsed.
int GetGdtfModeChannelCount(const std::string& gdtfPath,
                            const std::string& modeName);

// Returns the list of DMX channels and their functions for a mode in a GDTF
// file. Real channels use the first Offset value as channel number. Virtual
// channels (Offset=None/missing) set isVirtual=true and channel=0.
std::vector<GdtfChannelInfo> GetGdtfModeChannels(
    const std::string& gdtfPath,
    const std::string& modeName);

// Returns the list of DMX mode names defined in a GDTF file. Returns an empty
// vector when the file cannot be parsed or contains no modes.
std::vector<std::string> GetGdtfModes(const std::string& gdtfPath);

// Returns the fixture type name defined in a GDTF file. Returns an empty
// string when the name cannot be determined or the file cannot be parsed.
std::string GetGdtfFixtureName(const std::string& gdtfPath);

// Parses weight and power consumption from a GDTF file. Returns true if the
// file could be read. Values are set to zero when not specified.
bool GetGdtfProperties(const std::string& gdtfPath,
                       float& outWeightKg,
                       float& outPowerW);

// Returns the default model color defined in a GDTF file as a hex RGB string
// (e.g., "#RRGGBB"). Returns an empty string when no color is specified or
// the file cannot be parsed.
std::string GetGdtfModelColor(const std::string& gdtfPath);

// Updates Weight/PowerConsumption values in description.xml and appends a
// revision entry with timestamp and modifier metadata.
// Behavior and non-regression criteria are documented in
// docs/gdtf_mutation_policy.md.
bool SetGdtfProperties(const std::string& gdtfPath,
                       float weightKg,
                       float powerW,
                       const std::string& modifiedByProgram);
