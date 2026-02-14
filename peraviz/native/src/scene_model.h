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

struct SceneNode {
    std::string node_id;
    std::string parent_id;
    std::string name;
    std::string type;
    std::string asset_path;
    SceneTransform local_transform;
    bool is_fixture = false;
    bool is_axis = false;
    bool is_emitter = false;
};

struct SceneModel {
    std::vector<SceneNode> nodes;
    int fixture_count = 0;
    int truss_count = 0;
    int object_count = 0;
    int support_count = 0;
    int extracted_asset_count = 0;
    std::string cache_path;
};

} // namespace peraviz
