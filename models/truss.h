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

// Represents a truss object parsed from MVR
struct Truss {
    std::string uuid;
    std::string name;
    std::string gdtfSpec;
    std::string gdtfMode;
    std::string symbolFile; // 3D geometry file referenced by the Symbol
    std::string modelFile;  // Original model archive (.gtruss) if available
    std::string function;
    std::string layer;

    // Metadata fields
    std::string manufacturer;
    std::string model;
    float lengthMm = 0.0f;
    float widthMm = 0.0f;
    float heightMm = 0.0f;
    float weightKg = 0.0f;
    std::string crossSection;

    // Optional hang position reference
    std::string position;      // Position UUID or raw vector
    std::string positionName;  // Resolved name from AUXData

    int unitNumber = 0;
    int customId = 0;
    int customIdType = 0;

    Matrix transform;
};
