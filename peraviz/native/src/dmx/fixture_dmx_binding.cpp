#include "dmx/fixture_dmx_binding.h"

#include <algorithm>
#include <cctype>

namespace {

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

} // namespace

namespace peraviz::dmx {

FixtureBindingBuildResult build_dimmer_bindings(
    const std::vector<FixturePatch> &patches,
    int universe_offset,
    std::unordered_map<std::string, FixtureDimmerBinding> &fixture_lookup) {
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

        int dimmer_offset_1_based = -1;
        std::string debug_reason;
        if (!resolve_dimmer_channel_offset(patch.gdtf_path, patch.dmx_mode,
                                           dimmer_offset_1_based, debug_reason)) {
            result.unbound.push_back({patch.fixture_uuid, debug_reason});
            continue;
        }

        const int dmx_channel_index_0 = (patch.mvr_address - 1) + (dimmer_offset_1_based - 1);
        if (dmx_channel_index_0 < 0 || dmx_channel_index_0 >= 512) {
            result.unbound.push_back({patch.fixture_uuid, "resolved dimmer channel index out of 0..511 range"});
            continue;
        }

        FixtureDimmerBinding binding;
        binding.fixture_uuid = patch.fixture_uuid;
        binding.artnet_universe_id = patch.mvr_universe + universe_offset;
        binding.dmx_channel_index_0 = dmx_channel_index_0;
        binding.scale = 1.0F;

        result.bindings.push_back(binding);
        fixture_lookup[binding.fixture_uuid] = binding;
    }

    return result;
}

} // namespace peraviz::dmx
