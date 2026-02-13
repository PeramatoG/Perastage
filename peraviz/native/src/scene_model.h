#pragma once

#include <string>
#include <vector>

namespace peraviz {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct SceneTransform {
    Vec3 position;
    Vec3 rotation_degrees;
    Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct SceneInstance {
    std::string id;
    std::string type;
    SceneTransform transform;
    bool is_fixture = false;
};

struct SceneModel {
    std::vector<SceneInstance> instances;
    int fixture_count = 0;
    int truss_count = 0;
    int object_count = 0;
    int support_count = 0;
};

} // namespace peraviz
