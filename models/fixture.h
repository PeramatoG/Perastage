#pragma once

#include <string>

// Represents a lighting fixture object parsed from MVR
struct Fixture {
    std::string uuid;             // Unique identifier from the MVR file
    std::string name;             // Fixture name
    std::string gdtfSpec;         // GDTF file name
    std::string gdtfMode;         // GDTF mode name (optional)
    std::string focus;            // Focus reference UUID (optional)
    std::string function;         // Function string (optional)
    std::string layer;            // Layer to which the fixture belongs

    std::string position;         // Position reference UUID or raw vector (if applicable)
    std::string address;          // DMX address in string format (e.g., "1.1")
    std::string matrixRaw;        // Raw matrix string from XML (to be parsed later)

    int fixtureId = 0;            // FixtureID (numeric ID from XML)
    int fixtureIdNumeric = 0;     // Optional numeric ID field (if distinct)
    int unitNumber = 0;           // Unit number (if available)
    int customId = 0;             // Custom ID field
    int customIdType = 0;         // Custom ID type code

    bool dmxInvertPan = false;    // Pan inversion flag
    bool dmxInvertTilt = false;   // Tilt inversion flag
};
