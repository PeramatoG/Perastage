#pragma once

#include <unordered_map>
#include <string>
#include "fixture.h"
#include "truss.h"
#include "layer.h"

// Represents the full scene structure from an MVR file
class MvrScene {
public:
    void Clear();

    std::unordered_map<std::string, Fixture> fixtures;
    std::unordered_map<std::string, Truss> trusses;
    std::unordered_map<std::string, Layer> layers;

    std::string provider;
    std::string providerVersion;
    int versionMajor = 1;
    int versionMinor = 6;
};
