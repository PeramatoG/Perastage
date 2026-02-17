#include "peraviz_loader.h"

#include "mvr_scene_loader.h"
#include "mesh_3ds_loader.h"

#ifdef PERAVIZ_ENABLE_DMX
#include "dmx/fixture_dmx_binding.h"
#endif

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <unordered_map>

namespace {

godot::Array serialize_fixture_patches(const peraviz::SceneModel &model) {
    godot::Array out;
    out.resize(static_cast<int64_t>(model.fixture_patches.size()));
    int64_t index = 0;
    for (const auto &patch : model.fixture_patches) {
        godot::Dictionary d;
        d["fixture_uuid"] = godot::String(patch.fixture_uuid.c_str());
        d["mvr_universe"] = patch.mvr_universe;
        d["mvr_address"] = patch.mvr_address;
        d["dmx_mode"] = godot::String(patch.dmx_mode.c_str());
        d["gdtf_path"] = godot::String(patch.gdtf_path.c_str());
        out[index++] = d;
    }
    return out;
}

} // namespace

namespace godot {

void PeravizLoader::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_mvr", "path", "peraviz_debug_baseline", "peraviz_debug_coords"), &PeravizLoader::load_mvr);
    ClassDB::bind_method(D_METHOD("load_3ds_mesh_data", "path"), &PeravizLoader::load_3ds_mesh_data);
    ClassDB::bind_method(D_METHOD("get_fixtures_patch"), &PeravizLoader::get_fixtures_patch);
    ClassDB::bind_method(D_METHOD("build_fixture_dimmer_bindings", "universe_offset"),
                         &PeravizLoader::build_fixture_dimmer_bindings,
                         DEFVAL(-1));
}

Array PeravizLoader::load_mvr(const String &path, bool peraviz_debug_baseline,
                              bool peraviz_debug_coords) {
    last_scene_model_ = peraviz::load_mvr(std::string(path.utf8().get_data()),
                                          peraviz_debug_baseline,
                                          peraviz_debug_coords);

    UtilityFunctions::print("[PeravizNative] load_mvr nodes=", last_scene_model_.nodes.size(),
                            " baseline_debug=", peraviz_debug_baseline,
                            " coords_debug=", peraviz_debug_coords,
                            " fixtures=", last_scene_model_.fixture_count,
                            " fixture_patches=", last_scene_model_.fixture_patches.size(),
                            " trusses=", last_scene_model_.truss_count,
                            " objects=", last_scene_model_.object_count,
                            " supports=", last_scene_model_.support_count,
                            " extracted_assets=", last_scene_model_.extracted_asset_count,
                            " cache=", String(last_scene_model_.cache_path.c_str()));

    Array out;
    out.resize(static_cast<int64_t>(last_scene_model_.nodes.size()));
    int index = 0;
    for (const auto &node : last_scene_model_.nodes) {
        Dictionary d;
        d["node_id"] = String(node.node_id.c_str());
        d["parent_id"] = String(node.parent_id.c_str());
        d["name"] = String(node.name.c_str());
        d["type"] = String(node.type.c_str());
        d["class"] = String(node.node_class.c_str());
        d["asset_kind"] = String(node.asset_kind.c_str());
        d["asset_path"] = String(node.asset_path.c_str());
        d["is_fixture"] = node.is_fixture;
        d["is_axis"] = node.is_axis;
        d["is_emitter"] = node.is_emitter;
        d["has_luminous_flux"] = node.has_luminous_flux;
        d["luminous_flux"] = node.luminous_flux;
        d["has_color_temperature"] = node.has_color_temperature;
        d["color_temperature"] = node.color_temperature;
        d["has_beam_angle"] = node.has_beam_angle;
        d["beam_angle"] = node.beam_angle;
        d["has_field_angle"] = node.has_field_angle;
        d["field_angle"] = node.field_angle;
        d["has_beam_radius"] = node.has_beam_radius;
        d["beam_radius"] = node.beam_radius;
        d["has_dominant_wavelength"] = node.has_dominant_wavelength;
        d["dominant_wavelength"] = node.dominant_wavelength;
        d["pos"] = Vector3(node.local_transform.position.x, node.local_transform.position.y,
                            node.local_transform.position.z);
        d["rot"] = Vector3(node.local_transform.rotation_degrees.x,
                            node.local_transform.rotation_degrees.y,
                            node.local_transform.rotation_degrees.z);
        d["scale"] = Vector3(node.local_transform.scale.x, node.local_transform.scale.y,
                              node.local_transform.scale.z);
        d["basis_x"] = Vector3(node.local_transform.basis_x.x, node.local_transform.basis_x.y,
                                node.local_transform.basis_x.z);
        d["basis_y"] = Vector3(node.local_transform.basis_y.x, node.local_transform.basis_y.y,
                                node.local_transform.basis_y.z);
        d["basis_z"] = Vector3(node.local_transform.basis_z.x, node.local_transform.basis_z.y,
                                node.local_transform.basis_z.z);
        d["has_basis"] = node.local_transform.has_basis;
        out[index++] = d;
    }
    return out;
}

Dictionary PeravizLoader::load_3ds_mesh_data(const String &path) const {
    PackedVector3Array vertices;
    PackedVector3Array normals;
    PackedInt32Array indices;
    String error;

    Dictionary out;
    const bool ok = peraviz::load_3ds_mesh_data(path, vertices, normals, indices, error);
    out["ok"] = ok;
    if (!ok) {
        out["error"] = error;
        return out;
    }

    out["vertices"] = vertices;
    out["normals"] = normals;
    out["indices"] = indices;
    return out;
}

Array PeravizLoader::get_fixtures_patch() const {
    return serialize_fixture_patches(last_scene_model_);
}

Dictionary PeravizLoader::build_fixture_dimmer_bindings(int universe_offset) const {
    Dictionary out;
    out["universe_offset"] = universe_offset;

#ifdef PERAVIZ_ENABLE_DMX
    std::vector<peraviz::dmx::FixturePatch> patches;
    patches.reserve(last_scene_model_.fixture_patches.size());
    for (const auto &patch : last_scene_model_.fixture_patches) {
        peraviz::dmx::FixturePatch copy;
        copy.fixture_uuid = patch.fixture_uuid;
        copy.mvr_universe = patch.mvr_universe;
        copy.mvr_address = patch.mvr_address;
        copy.dmx_mode = patch.dmx_mode;
        copy.gdtf_path = patch.gdtf_path;
        patches.push_back(copy);
    }

    std::unordered_map<std::string, peraviz::dmx::FixtureControlBinding> lookup;
    const peraviz::dmx::FixtureBindingBuildResult result =
        peraviz::dmx::build_dimmer_bindings(patches, universe_offset, lookup);

    Array bindings;
    bindings.resize(static_cast<int64_t>(result.bindings.size()));
    int64_t binding_index = 0;
    for (const auto &binding : result.bindings) {
        Dictionary item;
        item["fixture_uuid"] = String(binding.fixture_uuid.c_str());
        item["artnet_universe_id"] = binding.artnet_universe_id;
        item["dimmer_channel_index_0"] = binding.dimmer.coarse_dmx_channel_index_0;
        item["dimmer_fine_channel_index_0"] = binding.dimmer.fine_dmx_channel_index_0;
        item["pan_channel_index_0"] = binding.pan.coarse_dmx_channel_index_0;
        item["pan_fine_channel_index_0"] = binding.pan.fine_dmx_channel_index_0;
        item["tilt_channel_index_0"] = binding.tilt.coarse_dmx_channel_index_0;
        item["tilt_fine_channel_index_0"] = binding.tilt.fine_dmx_channel_index_0;
        item["scale"] = binding.scale;
        bindings[binding_index++] = item;
    }

    Array unbound;
    unbound.resize(static_cast<int64_t>(result.unbound.size()));
    int64_t unbound_index = 0;
    for (const auto &item : result.unbound) {
        Dictionary d;
        d["fixture_uuid"] = String(item.fixture_uuid.c_str());
        d["reason"] = String(item.reason.c_str());
        unbound[unbound_index++] = d;
    }

    out["bindings"] = bindings;
    out["unbound"] = unbound;
#else
    out["bindings"] = Array();
    out["unbound"] = Array();
#endif
    return out;
}

} // namespace godot
