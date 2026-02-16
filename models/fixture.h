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
#include "types.h"

// Represents a lighting fixture object parsed from MVR
struct Fixture {
    std::string uuid;             // Unique identifier from the MVR file
    std::string instanceName;     // Name of this fixture instance (from MVR)
    std::string typeName;         // GDTF fixture type name
    std::string gdtfSpec;         // GDTF file name
    std::string gdtfMode;         // GDTF mode name (optional)
    std::string focus;            // Focus reference UUID (optional)
    std::string function;         // Function string (optional)
    std::string layer;            // Layer to which the fixture belongs

    std::string position;         // Position reference UUID or raw vector (if applicable)
    std::string positionName;     // Resolved position name if available
    std::string address;          // DMX address in string format (e.g., "1.1")
    std::string matrixRaw;        // Raw matrix string from XML (to be parsed later)
    Matrix transform;             // Parsed transformation matrix

    std::string color;            // Hex RGB color (e.g., "#RRGGBB")

    std::string fixtureIdText;    // FixtureID (free-form string identifier from XML)
    int fixtureId = 0;            // Numeric FixtureID fallback used internally
    int fixtureIdNumeric = 0;     // Optional numeric ID field (if distinct)
    int unitNumber = 0;           // Unit number (if available)
    int customId = 0;             // Custom ID field
    int customIdType = 0;         // Custom ID type code

    bool dmxInvertPan = false;    // Pan inversion flag
    bool dmxInvertTilt = false;   // Tilt inversion flag

    float powerConsumptionW = 0.0f; // Power consumption in watts
    float weightKg = 0.0f;          // Fixture weight in kilograms

    // Convenience method to access translation as array
    std::array<float,3> GetPosition() const { return transform.o; }
};
