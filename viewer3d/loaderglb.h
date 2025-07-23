#pragma once
#include <string>
#include "mesh.h"

// Minimal loader for GLB (glTF 2.0 binary) files. Only reads the first mesh
// and primitive, extracting vertex positions and triangle indices.
bool LoadGLB(const std::string& path, Mesh& outMesh);
