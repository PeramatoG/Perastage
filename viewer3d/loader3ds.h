#pragma once
#include <string>
#include "mesh.h"

// Very small parser for Discreet 3DS files loading only vertex and face data.
bool Load3DS(const std::string& path, Mesh& outMesh);
