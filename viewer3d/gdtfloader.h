#pragma once

#include <string>
#include <vector>
#include "mesh.h"
#include "types.h"

struct GdtfObject {
    Mesh mesh;
    Matrix transform; // local transform relative to fixture
};

// Loads the models defined in a GDTF file. Returns true on success.
bool LoadGdtf(const std::string& gdtfPath, std::vector<GdtfObject>& outObjects);
