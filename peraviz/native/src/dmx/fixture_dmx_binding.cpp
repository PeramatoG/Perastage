#include "dmx/fixture_dmx_binding.h"

#include <utility>

namespace peraviz::dmx {

namespace {

int to_channel_index_0(const FixturePatch &patch, int offset_1_based) {
    if (offset_1_based <= 0) {
        return -1;
    }
    return (patch.mvr_address - 1) + (offset_1_based - 1);
}

bool is_valid_channel_index(int index_0) {
    return index_0 >= 0 && index_0 < 512;
}


FixtureAttributeChannel to_channel_binding(const FixturePatch &patch,
                                          int coarse_1_based,
                                          int fine_1_based,
                                          int ultra_1_based) {
    FixtureAttributeChannel out;
    out.coarse_dmx_channel_index_0 = to_channel_index_0(patch, coarse_1_based);
    out.fine_dmx_channel_index_0 = to_channel_index_0(patch, fine_1_based);
    out.ultra_fine_dmx_channel_index_0 = to_channel_index_0(patch, ultra_1_based);
    return out;
}

void sanitize_channel_binding(FixtureAttributeChannel &channel) {
    if (channel.fine_dmx_channel_index_0 >= 0 && !is_valid_channel_index(channel.fine_dmx_channel_index_0)) {
        channel.fine_dmx_channel_index_0 = -1;
    }
    if (channel.ultra_fine_dmx_channel_index_0 >= 0 &&
        !is_valid_channel_index(channel.ultra_fine_dmx_channel_index_0)) {
        channel.ultra_fine_dmx_channel_index_0 = -1;
    }
}

} // namespace

FixtureBindingBuildResult build_fixture_control_bindings(
    const std::vector<FixturePatch> &patches,
    int universe_offset,
    std::unordered_map<std::string, FixtureControlBinding> &fixture_lookup) {
    FixtureBindingBuildResult result;
    fixture_lookup.clear();

    for (const FixturePatch &patch : patches) {
        if (patch.fixture_uuid.empty()) {
            continue;
        }
        if (patch.mvr_universe <= 0 || patch.mvr_address <= 0 || patch.mvr_address > 512) {
            result.unbound.push_back({patch.fixture_uuid, "missing or invalid MVR patch (universe/address)"});
            continue;
        }
        if (patch.dmx_mode.empty()) {
            result.unbound.push_back({patch.fixture_uuid, "missing DMX mode in MVR fixture"});
            continue;
        }
        if (patch.gdtf_path.empty()) {
            result.unbound.push_back({patch.fixture_uuid, "missing resolved GDTF path"});
            continue;
        }

        FixtureControlOffsets offsets;
        std::string debug_reason;
        if (!resolve_fixture_control_offsets(patch.gdtf_path, patch.dmx_mode,
                                             offsets, debug_reason)) {
            result.unbound.push_back({patch.fixture_uuid, debug_reason});
            continue;
        }

        FixtureControlBinding binding;
        binding.fixture_uuid = patch.fixture_uuid;
        binding.artnet_universe_id = patch.mvr_universe + universe_offset;
        binding.scale = 1.0F;

        binding.dimmer.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.dimmer_coarse_offset_1_based);
        binding.dimmer.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.dimmer_fine_offset_1_based);
        binding.dimmer.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.dimmer_ultra_fine_offset_1_based);
        binding.pan.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.pan_coarse_offset_1_based);
        binding.pan.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.pan_fine_offset_1_based);
        binding.pan.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.pan_ultra_fine_offset_1_based);
        binding.tilt.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.tilt_coarse_offset_1_based);
        binding.tilt.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.tilt_fine_offset_1_based);
        binding.tilt.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.tilt_ultra_fine_offset_1_based);
        binding.zoom.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.zoom_coarse_offset_1_based);
        binding.zoom.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.zoom_fine_offset_1_based);
        binding.zoom.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.zoom_ultra_fine_offset_1_based);
        binding.cyan.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.cyan_coarse_offset_1_based);
        binding.cyan.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.cyan_fine_offset_1_based);
        binding.cyan.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.cyan_ultra_fine_offset_1_based);
        binding.magenta.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.magenta_coarse_offset_1_based);
        binding.magenta.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.magenta_fine_offset_1_based);
        binding.magenta.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.magenta_ultra_fine_offset_1_based);
        binding.yellow.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.yellow_coarse_offset_1_based);
        binding.yellow.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.yellow_fine_offset_1_based);
        binding.yellow.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.yellow_ultra_fine_offset_1_based);
        binding.strobe.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.strobe_coarse_offset_1_based);
        binding.strobe.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.strobe_fine_offset_1_based);
        binding.strobe.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.strobe_ultra_fine_offset_1_based);
        binding.prism.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_coarse_offset_1_based);
        binding.prism.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_fine_offset_1_based);
        binding.prism.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_ultra_fine_offset_1_based);
        binding.prism_index.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_index_coarse_offset_1_based);
        binding.prism_index.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_index_fine_offset_1_based);
        binding.prism_index.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_index_ultra_fine_offset_1_based);
        binding.prism_rotation.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_rotation_coarse_offset_1_based);
        binding.prism_rotation.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_rotation_fine_offset_1_based);
        binding.prism_rotation.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.prism_rotation_ultra_fine_offset_1_based);
        binding.color_wheel.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.color_wheel_coarse_offset_1_based);
        binding.color_wheel.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.color_wheel_fine_offset_1_based);
        binding.color_wheel.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.color_wheel_ultra_fine_offset_1_based);
        binding.color_wheel_rotation.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.color_wheel_rotation_coarse_offset_1_based);
        binding.color_wheel_rotation.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.color_wheel_rotation_fine_offset_1_based);
        binding.color_wheel_rotation.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.color_wheel_rotation_ultra_fine_offset_1_based);
        binding.animation_wheel.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.animation_wheel_coarse_offset_1_based);
        binding.animation_wheel.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.animation_wheel_fine_offset_1_based);
        binding.animation_wheel.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.animation_wheel_ultra_fine_offset_1_based);
        binding.animation_wheel_rotation.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.animation_wheel_rotation_coarse_offset_1_based);
        binding.animation_wheel_rotation.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.animation_wheel_rotation_fine_offset_1_based);
        binding.animation_wheel_rotation.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.animation_wheel_rotation_ultra_fine_offset_1_based);
        binding.gobo.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_coarse_offset_1_based);
        binding.gobo.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_fine_offset_1_based);
        binding.gobo.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_ultra_fine_offset_1_based);
        binding.gobo_index.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_index_coarse_offset_1_based);
        binding.gobo_index.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_index_fine_offset_1_based);
        binding.gobo_index.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_index_ultra_fine_offset_1_based);
        binding.gobo_rotation.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_rotation_coarse_offset_1_based);
        binding.gobo_rotation.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_rotation_fine_offset_1_based);
        binding.gobo_rotation.ultra_fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.gobo_rotation_ultra_fine_offset_1_based);
        binding.gobo_wheel_number = offsets.gobo_wheel_number;
        binding.gobo_wheel_name = offsets.gobo_wheel_name;
        binding.gobo_slots = offsets.gobo_slots;
        binding.gobo_ranges = offsets.gobo_ranges;
        binding.gobo_wheels.reserve(offsets.gobo_wheels.size());
        for (const FixtureGoboWheelOffset &wheel : offsets.gobo_wheels) {
            FixtureGoboWheelBinding wheel_binding;
            wheel_binding.channel = to_channel_binding(patch,
                                                       wheel.coarse_offset_1_based,
                                                       wheel.fine_offset_1_based,
                                                       wheel.ultra_fine_offset_1_based);
            wheel_binding.index_channel = to_channel_binding(patch,
                                                             wheel.index_coarse_offset_1_based,
                                                             wheel.index_fine_offset_1_based,
                                                             wheel.index_ultra_fine_offset_1_based);
            wheel_binding.rotation_channel = to_channel_binding(patch,
                                                                wheel.rotation_coarse_offset_1_based,
                                                                wheel.rotation_fine_offset_1_based,
                                                                wheel.rotation_ultra_fine_offset_1_based);
            wheel_binding.supports_index = wheel.supports_index;
            wheel_binding.supports_rotation = wheel.supports_rotation;
            wheel_binding.supports_spin_rotation = wheel.supports_spin_rotation;
            wheel_binding.supports_shake = wheel.supports_shake;
            wheel_binding.has_index_physical_limits = wheel.has_index_physical_limits;
            wheel_binding.index_physical_min = wheel.index_physical_min;
            wheel_binding.index_physical_max = wheel.index_physical_max;
            wheel_binding.rotation_ranges = wheel.rotation_ranges;
            wheel_binding.shake_ranges = wheel.shake_ranges;
            wheel_binding.wheel_number = wheel.wheel_number;
            wheel_binding.wheel_name = wheel.wheel_name;
            wheel_binding.slots = wheel.slots;
            wheel_binding.ranges = wheel.ranges;
            binding.gobo_wheels.push_back(std::move(wheel_binding));
        }
        binding.has_zoom_physical_limits = offsets.has_zoom_physical_limits;
        binding.zoom_physical_min_degrees = offsets.zoom_physical_min_degrees;
        binding.zoom_physical_max_degrees = offsets.zoom_physical_max_degrees;

        const bool has_dimmer = is_valid_channel_index(binding.dimmer.coarse_dmx_channel_index_0);
        const bool has_pan = is_valid_channel_index(binding.pan.coarse_dmx_channel_index_0);
        const bool has_tilt = is_valid_channel_index(binding.tilt.coarse_dmx_channel_index_0);
        const bool has_zoom = is_valid_channel_index(binding.zoom.coarse_dmx_channel_index_0);
        const bool has_cyan = is_valid_channel_index(binding.cyan.coarse_dmx_channel_index_0);
        const bool has_magenta = is_valid_channel_index(binding.magenta.coarse_dmx_channel_index_0);
        const bool has_yellow = is_valid_channel_index(binding.yellow.coarse_dmx_channel_index_0);
        const bool has_strobe = is_valid_channel_index(binding.strobe.coarse_dmx_channel_index_0);
        const bool has_prism = is_valid_channel_index(binding.prism.coarse_dmx_channel_index_0);
        const bool has_prism_index = is_valid_channel_index(binding.prism_index.coarse_dmx_channel_index_0);
        const bool has_prism_rotation = is_valid_channel_index(binding.prism_rotation.coarse_dmx_channel_index_0);
        const bool has_color_wheel = is_valid_channel_index(binding.color_wheel.coarse_dmx_channel_index_0);
        const bool has_color_wheel_rotation =
            is_valid_channel_index(binding.color_wheel_rotation.coarse_dmx_channel_index_0);
        const bool has_animation_wheel =
            is_valid_channel_index(binding.animation_wheel.coarse_dmx_channel_index_0);
        const bool has_animation_wheel_rotation =
            is_valid_channel_index(binding.animation_wheel_rotation.coarse_dmx_channel_index_0);
        bool has_gobo = is_valid_channel_index(binding.gobo.coarse_dmx_channel_index_0);
        for (const FixtureGoboWheelBinding &wheel_binding : binding.gobo_wheels) {
            if (is_valid_channel_index(wheel_binding.channel.coarse_dmx_channel_index_0)) {
                has_gobo = true;
                break;
            }
        }
        if (!has_dimmer && !has_pan && !has_tilt && !has_zoom && !has_cyan && !has_magenta &&
            !has_yellow && !has_strobe && !has_prism && !has_prism_index && !has_prism_rotation &&
            !has_color_wheel && !has_color_wheel_rotation && !has_animation_wheel &&
            !has_animation_wheel_rotation && !has_gobo) {
            result.unbound.push_back({patch.fixture_uuid,
                                      "resolved DMX channels are out of 0..511 range"});
            continue;
        }

        if (binding.dimmer.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.dimmer.fine_dmx_channel_index_0)) {
            binding.dimmer.fine_dmx_channel_index_0 = -1;
        }
        if (binding.dimmer.ultra_fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.dimmer.ultra_fine_dmx_channel_index_0)) {
            binding.dimmer.ultra_fine_dmx_channel_index_0 = -1;
        }
        if (binding.pan.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.pan.fine_dmx_channel_index_0)) {
            binding.pan.fine_dmx_channel_index_0 = -1;
        }
        if (binding.pan.ultra_fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.pan.ultra_fine_dmx_channel_index_0)) {
            binding.pan.ultra_fine_dmx_channel_index_0 = -1;
        }
        if (binding.tilt.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.tilt.fine_dmx_channel_index_0)) {
            binding.tilt.fine_dmx_channel_index_0 = -1;
        }
        if (binding.tilt.ultra_fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.tilt.ultra_fine_dmx_channel_index_0)) {
            binding.tilt.ultra_fine_dmx_channel_index_0 = -1;
        }
        if (binding.zoom.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.zoom.fine_dmx_channel_index_0)) {
            binding.zoom.fine_dmx_channel_index_0 = -1;
        }
        if (binding.zoom.ultra_fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.zoom.ultra_fine_dmx_channel_index_0)) {
            binding.zoom.ultra_fine_dmx_channel_index_0 = -1;
        }
        if (binding.cyan.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.cyan.fine_dmx_channel_index_0)) {
            binding.cyan.fine_dmx_channel_index_0 = -1;
        }
        if (binding.cyan.ultra_fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.cyan.ultra_fine_dmx_channel_index_0)) {
            binding.cyan.ultra_fine_dmx_channel_index_0 = -1;
        }
        if (binding.magenta.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.magenta.fine_dmx_channel_index_0)) {
            binding.magenta.fine_dmx_channel_index_0 = -1;
        }
        if (binding.magenta.ultra_fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.magenta.ultra_fine_dmx_channel_index_0)) {
            binding.magenta.ultra_fine_dmx_channel_index_0 = -1;
        }
        if (binding.yellow.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.yellow.fine_dmx_channel_index_0)) {
            binding.yellow.fine_dmx_channel_index_0 = -1;
        }
        if (binding.yellow.ultra_fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.yellow.ultra_fine_dmx_channel_index_0)) {
            binding.yellow.ultra_fine_dmx_channel_index_0 = -1;
        }
        sanitize_channel_binding(binding.strobe);
        sanitize_channel_binding(binding.prism);
        sanitize_channel_binding(binding.prism_index);
        sanitize_channel_binding(binding.prism_rotation);
        sanitize_channel_binding(binding.color_wheel);
        sanitize_channel_binding(binding.color_wheel_rotation);
        sanitize_channel_binding(binding.animation_wheel);
        sanitize_channel_binding(binding.animation_wheel_rotation);
        sanitize_channel_binding(binding.gobo);
        sanitize_channel_binding(binding.gobo_index);
        sanitize_channel_binding(binding.gobo_rotation);
        for (FixtureGoboWheelBinding &wheel_binding : binding.gobo_wheels) {
            sanitize_channel_binding(wheel_binding.channel);
            sanitize_channel_binding(wheel_binding.index_channel);
            sanitize_channel_binding(wheel_binding.rotation_channel);
        }

        result.bindings.push_back(binding);
        fixture_lookup[binding.fixture_uuid] = binding;
    }

    return result;
}


FixtureBindingBuildResult build_dimmer_bindings(
    const std::vector<FixturePatch> &patches,
    int universe_offset,
    std::unordered_map<std::string, FixtureControlBinding> &fixture_lookup) {
    return build_fixture_control_bindings(patches, universe_offset, fixture_lookup);
}

} // namespace peraviz::dmx
