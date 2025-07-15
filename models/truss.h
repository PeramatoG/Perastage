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

    int unitNumber = 0;
    int customId = 0;
    int customIdType = 0;

    Matrix transform;
};
