#pragma once

#include <string>
#include <vector>
#include "mesh.h"
#include "types.h"

struct GdtfObject {
    Mesh mesh;
    Matrix transform; // local transform relative to fixture
};

// Metadata for a GDTF model definition. Length/Width/Height correspond
// to the desired bounding box dimensions in meters as specified in the
// GDTF file. When any of these are zero the raw mesh size is used.
struct GdtfModelInfo {
    std::string file;
    float length = 0.0f; // meters
    float width  = 0.0f; // meters
    float height = 0.0f; // meters
};

// Loads the models defined in a GDTF file. Returns true on success.
bool LoadGdtf(const std::string& gdtfPath, std::vector<GdtfObject>& outObjects);
