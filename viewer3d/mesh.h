#pragma once
#include <vector>

struct Mesh {
    std::vector<float> vertices; // x,y,z order in mm
    std::vector<unsigned short> indices; // 3 indices per triangle
};
