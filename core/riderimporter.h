#pragma once

#include <string>

// Parses simple rider files (.txt/.pdf) to create dummy fixtures and trusses
class RiderImporter {
public:
    // Import rider located at path. Returns true on success.
    static bool Import(const std::string& path);
};

