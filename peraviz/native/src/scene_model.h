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
    Vec3 basis_x{1.0F, 0.0F, 0.0F};
    Vec3 basis_y{0.0F, 1.0F, 0.0F};
    Vec3 basis_z{0.0F, 0.0F, 1.0F};
    bool has_basis = false;
};

struct SceneNode {
    std::string node_id;
    std::string parent_id;
    std::string name;
    std::string type;
    std::string node_class;
    std::string asset_kind;
    std::string asset_path;
    std::string primitive_type;
    float primitive_size_x = 0.1F;
    float primitive_size_y = 0.1F;
    float primitive_size_z = 0.1F;
    SceneTransform local_transform;
    bool is_fixture = false;
    bool is_axis = false;
    bool is_emitter = false;
    bool is_lens = false;

    bool has_luminous_flux = false;
    float luminous_flux = 10000.0F;

    bool has_color_temperature = false;
    float color_temperature = 6000.0F;

    bool has_beam_angle = false;
    float beam_angle = 25.0F;

    bool has_field_angle = false;
    float field_angle = 25.0F;

    bool has_beam_radius = false;
    float beam_radius = 0.05F;

    bool has_dominant_wavelength = false;
    float dominant_wavelength = 0.0F;
};

struct SceneModel {
    std::vector<SceneNode> nodes;
    struct FixturePatch {
        std::string fixture_uuid;
        int mvr_universe = -1;
        int mvr_address = -1;
        std::string dmx_mode;
        std::string gdtf_path;
    };

    int fixture_count = 0;
    int truss_count = 0;
    int object_count = 0;
    int support_count = 0;
    int extracted_asset_count = 0;
    std::string cache_path;
    std::vector<FixturePatch> fixture_patches;
};

} // namespace peraviz
