#pragma once
#include <string>
#include "mesh.h"

// Minimal loader for GLB (glTF 2.0 binary) files. The loader parses all nodes
// and primitives of the file and applies the node transforms so that compound
// models are assembled correctly.
bool LoadGLB(const std::string& path, Mesh& outMesh);
