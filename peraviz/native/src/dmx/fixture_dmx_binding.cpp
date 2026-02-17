#include "dmx/fixture_dmx_binding.h"

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

} // namespace

FixtureBindingBuildResult build_dimmer_bindings(
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
        binding.pan.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.pan_coarse_offset_1_based);
        binding.pan.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.pan_fine_offset_1_based);
        binding.tilt.coarse_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.tilt_coarse_offset_1_based);
        binding.tilt.fine_dmx_channel_index_0 =
            to_channel_index_0(patch, offsets.tilt_fine_offset_1_based);

        const bool has_dimmer = is_valid_channel_index(binding.dimmer.coarse_dmx_channel_index_0);
        const bool has_pan = is_valid_channel_index(binding.pan.coarse_dmx_channel_index_0);
        const bool has_tilt = is_valid_channel_index(binding.tilt.coarse_dmx_channel_index_0);
        if (!has_dimmer && !has_pan && !has_tilt) {
            result.unbound.push_back({patch.fixture_uuid,
                                      "resolved DMX channels are out of 0..511 range"});
            continue;
        }

        if (binding.dimmer.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.dimmer.fine_dmx_channel_index_0)) {
            binding.dimmer.fine_dmx_channel_index_0 = -1;
        }
        if (binding.pan.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.pan.fine_dmx_channel_index_0)) {
            binding.pan.fine_dmx_channel_index_0 = -1;
        }
        if (binding.tilt.fine_dmx_channel_index_0 >= 0 &&
            !is_valid_channel_index(binding.tilt.fine_dmx_channel_index_0)) {
            binding.tilt.fine_dmx_channel_index_0 = -1;
        }

        result.bindings.push_back(binding);
        fixture_lookup[binding.fixture_uuid] = binding;
    }

    return result;
}

} // namespace peraviz::dmx
