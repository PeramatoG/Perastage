#pragma once

#include <string>
#include <vector>

// Represents a Layer (grouping element) in MVR
struct Layer {
    std::string uuid;
    std::string name;
    std::string color;            // Hex RGB color (e.g., "#RRGGBB")

    // UUIDs of child objects in this layer
    std::vector<std::string> childUUIDs;
};
