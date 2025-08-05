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

// Simple representation of a DMX channel/function pair
struct GdtfChannelInfo {
    int channel = 0;           // DMX channel number (coarse)
    std::string function;      // Associated function/attribute
};

// Loads the models defined in a GDTF file. Returns true on success.
bool LoadGdtf(const std::string& gdtfPath, std::vector<GdtfObject>& outObjects);

// Returns the number of DMX addresses used by the given mode in a GDTF file.
// Channels using more than one byte contribute multiple addresses to this
// count. Returns -1 when the mode cannot be found or the file cannot be
// parsed.
int GetGdtfModeChannelCount(const std::string& gdtfPath,
                            const std::string& modeName);

// Returns the list of DMX channels and their functions for a mode in a GDTF
// file. The channel number corresponds to the first Offset value when
// available, otherwise the sequential index is used.
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
