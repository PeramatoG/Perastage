#pragma once

#include <string>
#include "types.h"

// Represents a generic SceneObject parsed from MVR
struct SceneObject {
    std::string uuid;
    std::string name;
    std::string layer;
    std::string modelFile; // Referenced 3D model file (Geometry3D or Symdef)
    Matrix transform;
};
