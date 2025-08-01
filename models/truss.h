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
