#include "peraviz_loader.h"

#include "mvr_scene_loader.h"
#include "mesh_3ds_loader.h"

#ifdef PERAVIZ_ENABLE_DMX
#include "dmx/fixture_dmx_binding.h"
#endif

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
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
    ClassDB::bind_method(D_METHOD("build_fixture_dmx_bindings", "universe_offset"),
                         &PeravizLoader::build_fixture_dmx_bindings,
                         DEFVAL(-1));
    // Deprecated alias kept for one release to avoid breaking existing scripts/scenes.
    ClassDB::bind_method(D_METHOD("build_fixture_dimmer_bindings", "universe_offset"),
                         &PeravizLoader::build_fixture_dimmer_bindings,
                         DEFVAL(-1));
}

Array PeravizLoader::load_mvr(const String &path, bool peraviz_debug_baseline,
                              bool peraviz_debug_coords) {
    last_scene_model_ = peraviz::load_mvr(std::string(path.utf8().get_data()),
                                          peraviz_debug_baseline,
                                          peraviz_debug_coords);

    UtilityFunctions::print("[PeravizNative] load_mvr nodes=", static_cast<int64_t>(last_scene_model_.nodes.size()),
                            " baseline_debug=", peraviz_debug_baseline,
                            " coords_debug=", peraviz_debug_coords,
                            " fixtures=", last_scene_model_.fixture_count,
                            " fixture_patches=", static_cast<int64_t>(last_scene_model_.fixture_patches.size()),
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
        d["primitive_type"] = String(node.primitive_type.c_str());
        d["primitive_size_x"] = node.primitive_size_x;
        d["primitive_size_y"] = node.primitive_size_y;
        d["primitive_size_z"] = node.primitive_size_z;
        d["is_fixture"] = node.is_fixture;
        d["is_axis"] = node.is_axis;
        d["is_emitter"] = node.is_emitter;
        d["is_lens"] = node.is_lens;
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
    PackedVector2Array texcoords;
    PackedInt32Array indices;
    String texture_path;
    bool has_material_base_color = false;
    Vector3 material_base_color(1.0, 1.0, 1.0);
    String error;

    Dictionary out;
    const bool ok = peraviz::load_3ds_mesh_data(path, vertices, normals, texcoords, indices,
                                                texture_path, has_material_base_color,
                                                material_base_color, error);
    out["ok"] = ok;
    if (!ok) {
        out["error"] = error;
        return out;
    }

    out["vertices"] = vertices;
    out["normals"] = normals;
    out["texcoords"] = texcoords;
    out["indices"] = indices;
    out["texture_path"] = texture_path;
    out["has_material_base_color"] = has_material_base_color;
    out["material_base_color"] = material_base_color;
    return out;
}

Array PeravizLoader::get_fixtures_patch() const {
    return serialize_fixture_patches(last_scene_model_);
}

Dictionary PeravizLoader::build_fixture_dmx_bindings(int universe_offset) const {
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
        peraviz::dmx::build_fixture_control_bindings(patches, universe_offset, lookup);

    Array bindings;
    bindings.resize(static_cast<int64_t>(result.bindings.size()));
    int64_t binding_index = 0;
    for (const auto &binding : result.bindings) {
        Dictionary item;
        item["fixture_uuid"] = String(binding.fixture_uuid.c_str());
        item["artnet_universe_id"] = binding.artnet_universe_id;
        item["dimmer_channel_index_0"] = binding.dimmer.coarse_dmx_channel_index_0;
        item["dimmer_fine_channel_index_0"] = binding.dimmer.fine_dmx_channel_index_0;
        item["dimmer_ultra_fine_channel_index_0"] = binding.dimmer.ultra_fine_dmx_channel_index_0;
        item["pan_channel_index_0"] = binding.pan.coarse_dmx_channel_index_0;
        item["pan_fine_channel_index_0"] = binding.pan.fine_dmx_channel_index_0;
        item["pan_ultra_fine_channel_index_0"] = binding.pan.ultra_fine_dmx_channel_index_0;
        item["tilt_channel_index_0"] = binding.tilt.coarse_dmx_channel_index_0;
        item["tilt_fine_channel_index_0"] = binding.tilt.fine_dmx_channel_index_0;
        item["tilt_ultra_fine_channel_index_0"] = binding.tilt.ultra_fine_dmx_channel_index_0;
        item["zoom_channel_index_0"] = binding.zoom.coarse_dmx_channel_index_0;
        item["zoom_fine_channel_index_0"] = binding.zoom.fine_dmx_channel_index_0;
        item["zoom_ultra_fine_channel_index_0"] = binding.zoom.ultra_fine_dmx_channel_index_0;
        item["cyan_channel_index_0"] = binding.cyan.coarse_dmx_channel_index_0;
        item["cyan_fine_channel_index_0"] = binding.cyan.fine_dmx_channel_index_0;
        item["cyan_ultra_fine_channel_index_0"] = binding.cyan.ultra_fine_dmx_channel_index_0;
        item["magenta_channel_index_0"] = binding.magenta.coarse_dmx_channel_index_0;
        item["magenta_fine_channel_index_0"] = binding.magenta.fine_dmx_channel_index_0;
        item["magenta_ultra_fine_channel_index_0"] = binding.magenta.ultra_fine_dmx_channel_index_0;
        item["yellow_channel_index_0"] = binding.yellow.coarse_dmx_channel_index_0;
        item["yellow_fine_channel_index_0"] = binding.yellow.fine_dmx_channel_index_0;
        item["yellow_ultra_fine_channel_index_0"] = binding.yellow.ultra_fine_dmx_channel_index_0;
        item["gobo_channel_index_0"] = binding.gobo.coarse_dmx_channel_index_0;
        item["gobo_fine_channel_index_0"] = binding.gobo.fine_dmx_channel_index_0;
        item["gobo_ultra_fine_channel_index_0"] = binding.gobo.ultra_fine_dmx_channel_index_0;
        item["gobo_index_channel_index_0"] = binding.gobo_index.coarse_dmx_channel_index_0;
        item["gobo_index_fine_channel_index_0"] = binding.gobo_index.fine_dmx_channel_index_0;
        item["gobo_index_ultra_fine_channel_index_0"] = binding.gobo_index.ultra_fine_dmx_channel_index_0;
        item["gobo_rotation_channel_index_0"] = binding.gobo_rotation.coarse_dmx_channel_index_0;
        item["gobo_rotation_fine_channel_index_0"] = binding.gobo_rotation.fine_dmx_channel_index_0;
        item["gobo_rotation_ultra_fine_channel_index_0"] = binding.gobo_rotation.ultra_fine_dmx_channel_index_0;
        item["gobo_wheel_number"] = binding.gobo_wheel_number;
        item["gobo_wheel_name"] = String(binding.gobo_wheel_name.c_str());

        item["gobo1_channel_index_0"] = binding.gobo.coarse_dmx_channel_index_0;
        item["gobo1_fine_channel_index_0"] = binding.gobo.fine_dmx_channel_index_0;
        item["gobo1_ultra_fine_channel_index_0"] = binding.gobo.ultra_fine_dmx_channel_index_0;
        item["gobo1_wheel_name"] = String(binding.gobo_wheel_name.c_str());

        Array gobo_slots;
        gobo_slots.resize(static_cast<int64_t>(binding.gobo_slots.size()));
        for (int64_t slot_index = 0; slot_index < static_cast<int64_t>(binding.gobo_slots.size()); ++slot_index) {
            const auto &slot = binding.gobo_slots[static_cast<size_t>(slot_index)];
            Dictionary slot_item;
            slot_item["slot_index"] = slot.slot_index;
            slot_item["image_path"] = String(slot.image_path.c_str());
            gobo_slots[slot_index] = slot_item;
        }
        item["gobo_slots"] = gobo_slots;
        item["gobo1_slots"] = gobo_slots;

        Array gobo_ranges;
        gobo_ranges.resize(static_cast<int64_t>(binding.gobo_ranges.size()));
        for (int64_t range_index = 0; range_index < static_cast<int64_t>(binding.gobo_ranges.size()); ++range_index) {
            const auto &range = binding.gobo_ranges[static_cast<size_t>(range_index)];
            Dictionary range_item;
            range_item["dmx_from"] = range.dmx_from;
            range_item["dmx_to"] = range.dmx_to;
            range_item["mode_from_8bit"] = range.mode_from_8bit;
            range_item["mode_to_8bit"] = range.mode_to_8bit;
            range_item["slot_index"] = range.slot_index;
            range_item["behavior"] = static_cast<int>(range.behavior);
            gobo_ranges[range_index] = range_item;
        }
        item["gobo_ranges"] = gobo_ranges;
        item["gobo1_ranges"] = gobo_ranges;

        Array gobo_wheels;
        gobo_wheels.resize(static_cast<int64_t>(binding.gobo_wheels.size()));
        for (int64_t wheel_index = 0; wheel_index < static_cast<int64_t>(binding.gobo_wheels.size()); ++wheel_index) {
            const auto &wheel = binding.gobo_wheels[static_cast<size_t>(wheel_index)];
            Dictionary wheel_item;
            wheel_item["wheel_number"] = wheel.wheel_number;
            wheel_item["wheel_name"] = String(wheel.wheel_name.c_str());
            wheel_item["channel_index_0"] = wheel.channel.coarse_dmx_channel_index_0;
            wheel_item["fine_channel_index_0"] = wheel.channel.fine_dmx_channel_index_0;
            wheel_item["ultra_fine_channel_index_0"] = wheel.channel.ultra_fine_dmx_channel_index_0;
            wheel_item["index_channel_index_0"] = wheel.index_channel.coarse_dmx_channel_index_0;
            wheel_item["index_fine_channel_index_0"] = wheel.index_channel.fine_dmx_channel_index_0;
            wheel_item["index_ultra_fine_channel_index_0"] = wheel.index_channel.ultra_fine_dmx_channel_index_0;
            wheel_item["rotation_channel_index_0"] = wheel.rotation_channel.coarse_dmx_channel_index_0;
            wheel_item["rotation_fine_channel_index_0"] = wheel.rotation_channel.fine_dmx_channel_index_0;
            wheel_item["rotation_ultra_fine_channel_index_0"] = wheel.rotation_channel.ultra_fine_dmx_channel_index_0;
            wheel_item["supports_index"] = wheel.supports_index;
            wheel_item["supports_rotation"] = wheel.supports_rotation;
            wheel_item["supports_spin_rotation"] = wheel.supports_spin_rotation;
            wheel_item["supports_shake"] = wheel.supports_shake;
            wheel_item["has_index_physical_limits"] = wheel.has_index_physical_limits;
            wheel_item["index_physical_min"] = wheel.index_physical_min;
            wheel_item["index_physical_max"] = wheel.index_physical_max;
            wheel_item["rotation_source_channel"] = String("select");
            wheel_item["rotation_raw_coarse"] = -1;
            wheel_item["rotation_raw_fine"] = -1;
            wheel_item["rotation_raw_8bit"] = -1;
            wheel_item["rotation_mode_window_from"] = -1;
            wheel_item["rotation_mode_window_to"] = -1;
            wheel_item["rotation_matched_range_start"] = -1;
            wheel_item["rotation_matched_range_end"] = -1;
            wheel_item["rotation_matched_physical_from"] = 0.0;
            wheel_item["rotation_matched_physical_to"] = 0.0;
            wheel_item["rotation_is_stop_range"] = false;

            Array rotation_ranges;
            rotation_ranges.resize(static_cast<int64_t>(wheel.rotation_ranges.size()));
            for (int64_t rotation_range_index = 0; rotation_range_index < static_cast<int64_t>(wheel.rotation_ranges.size()); ++rotation_range_index) {
                const auto &rotation_range = wheel.rotation_ranges[static_cast<size_t>(rotation_range_index)];
                Dictionary rotation_range_item;
                rotation_range_item["dmx_from"] = rotation_range.dmx_from;
                rotation_range_item["dmx_to"] = rotation_range.dmx_to;
                rotation_range_item["mode_from_8bit"] = rotation_range.mode_from_8bit;
                rotation_range_item["mode_to_8bit"] = rotation_range.mode_to_8bit;
                rotation_range_item["physical_from"] = rotation_range.physical_from;
                rotation_range_item["physical_to"] = rotation_range.physical_to;
                rotation_range_item["is_stop_range"] = rotation_range.is_stop_range;
                rotation_range_item["is_rotation_channel_range"] = rotation_range.is_rotation_channel_range;
                rotation_ranges[rotation_range_index] = rotation_range_item;
            }
            wheel_item["rotation_ranges"] = rotation_ranges;

            Array shake_ranges;
            shake_ranges.resize(static_cast<int64_t>(wheel.shake_ranges.size()));
            for (int64_t shake_range_index = 0; shake_range_index < static_cast<int64_t>(wheel.shake_ranges.size()); ++shake_range_index) {
                const auto &shake_range = wheel.shake_ranges[static_cast<size_t>(shake_range_index)];
                Dictionary shake_range_item;
                shake_range_item["dmx_from"] = shake_range.dmx_from;
                shake_range_item["dmx_to"] = shake_range.dmx_to;
                shake_range_item["mode_from_8bit"] = shake_range.mode_from_8bit;
                shake_range_item["mode_to_8bit"] = shake_range.mode_to_8bit;
                shake_range_item["physical_from"] = shake_range.physical_from;
                shake_range_item["physical_to"] = shake_range.physical_to;
                shake_range_item["has_explicit_amplitude"] = shake_range.has_explicit_amplitude;
                shake_range_item["amplitude_from_degrees"] = shake_range.amplitude_from_degrees;
                shake_range_item["amplitude_to_degrees"] = shake_range.amplitude_to_degrees;
                shake_range_item["slot_index"] = shake_range.slot_index;
                shake_range_item["control_type"] = static_cast<int>(shake_range.control_type);
                shake_ranges[shake_range_index] = shake_range_item;
            }
            wheel_item["shake_ranges"] = shake_ranges;

            Array wheel_slots;
            wheel_slots.resize(static_cast<int64_t>(wheel.slots.size()));
            for (int64_t slot_index = 0; slot_index < static_cast<int64_t>(wheel.slots.size()); ++slot_index) {
                const auto &slot = wheel.slots[static_cast<size_t>(slot_index)];
                Dictionary slot_item;
                slot_item["slot_index"] = slot.slot_index;
                slot_item["image_path"] = String(slot.image_path.c_str());
                wheel_slots[slot_index] = slot_item;
            }
            wheel_item["slots"] = wheel_slots;

            Array wheel_ranges;
            wheel_ranges.resize(static_cast<int64_t>(wheel.ranges.size()));
            for (int64_t range_index = 0; range_index < static_cast<int64_t>(wheel.ranges.size()); ++range_index) {
                const auto &range = wheel.ranges[static_cast<size_t>(range_index)];
                Dictionary range_item;
                range_item["dmx_from"] = range.dmx_from;
                range_item["dmx_to"] = range.dmx_to;
                range_item["mode_from_8bit"] = range.mode_from_8bit;
                range_item["mode_to_8bit"] = range.mode_to_8bit;
                range_item["slot_index"] = range.slot_index;
                range_item["behavior"] = static_cast<int>(range.behavior);
                wheel_ranges[range_index] = range_item;
            }
            wheel_item["ranges"] = wheel_ranges;
            gobo_wheels[wheel_index] = wheel_item;
        }
        item["gobo_wheels"] = gobo_wheels;

        item["has_zoom_physical_limits"] = binding.has_zoom_physical_limits;
        item["zoom_physical_min_degrees"] = binding.zoom_physical_min_degrees;
        item["zoom_physical_max_degrees"] = binding.zoom_physical_max_degrees;
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

Dictionary PeravizLoader::build_fixture_dimmer_bindings(int universe_offset) const {
    UtilityFunctions::push_warning("[PeravizNative] build_fixture_dimmer_bindings is deprecated; use build_fixture_dmx_bindings instead.");
    return build_fixture_dmx_bindings(universe_offset);
}

} // namespace godot
