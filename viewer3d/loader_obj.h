#pragma once

#include "mesh.h"

#include <string>

// Loads an OBJ mesh with positions/normals/UVs and computes missing attributes when needed.
bool LoadOBJ(const std::string &path, Mesh &outMesh, std::string *errorMessage = nullptr);

// Converts an OBJ file to a minimal GLB file used as the official persisted model format.
bool ConvertObjFileToGlb(const std::string &objPath, const std::string &glbPath,
                         std::string *errorMessage = nullptr);
