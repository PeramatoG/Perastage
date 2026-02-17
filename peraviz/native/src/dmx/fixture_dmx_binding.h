#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace peraviz::dmx {

struct FixturePatch {
    std::string fixture_uuid;
    int mvr_universe = -1;
    int mvr_address = -1;
    std::string dmx_mode;
    std::string gdtf_path;
};

struct FixtureDimmerBinding {
    std::string fixture_uuid;
    int artnet_universe_id = -1;
    int dmx_channel_index_0 = -1;
    float scale = 1.0F;
};

struct FixtureUnboundReason {
    std::string fixture_uuid;
    std::string reason;
};

struct FixtureBindingBuildResult {
    std::vector<FixtureDimmerBinding> bindings;
    std::vector<FixtureUnboundReason> unbound;
};

bool resolve_dimmer_channel_offset(const std::string &gdtf_path,
                                   const std::string &dmx_mode_name,
                                   int &out_offset_1_based,
                                   std::string &out_debug_reason);

FixtureBindingBuildResult build_dimmer_bindings(
    const std::vector<FixturePatch> &patches,
    int universe_offset,
    std::unordered_map<std::string, FixtureDimmerBinding> &fixture_lookup);

} // namespace peraviz::dmx
