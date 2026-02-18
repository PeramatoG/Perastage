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

struct FixtureAttributeChannel {
    int coarse_dmx_channel_index_0 = -1;
    int fine_dmx_channel_index_0 = -1;
    int ultra_fine_dmx_channel_index_0 = -1;
};

struct FixtureControlBinding {
    std::string fixture_uuid;
    int artnet_universe_id = -1;
    FixtureAttributeChannel dimmer;
    FixtureAttributeChannel pan;
    FixtureAttributeChannel tilt;
    FixtureAttributeChannel zoom;
    bool has_zoom_physical_limits = false;
    float zoom_physical_min_degrees = -1.0F;
    float zoom_physical_max_degrees = -1.0F;
    float scale = 1.0F;
};

struct FixtureUnboundReason {
    std::string fixture_uuid;
    std::string reason;
};

struct FixtureBindingBuildResult {
    std::vector<FixtureControlBinding> bindings;
    std::vector<FixtureUnboundReason> unbound;
};

struct FixtureControlOffsets {
    int dimmer_coarse_offset_1_based = -1;
    int dimmer_fine_offset_1_based = -1;
    int dimmer_ultra_fine_offset_1_based = -1;
    int pan_coarse_offset_1_based = -1;
    int pan_fine_offset_1_based = -1;
    int pan_ultra_fine_offset_1_based = -1;
    int tilt_coarse_offset_1_based = -1;
    int tilt_fine_offset_1_based = -1;
    int tilt_ultra_fine_offset_1_based = -1;
    int zoom_coarse_offset_1_based = -1;
    int zoom_fine_offset_1_based = -1;
    int zoom_ultra_fine_offset_1_based = -1;
    bool has_zoom_physical_limits = false;
    float zoom_physical_min_degrees = -1.0F;
    float zoom_physical_max_degrees = -1.0F;

    bool has_any() const {
        return dimmer_coarse_offset_1_based > 0 || pan_coarse_offset_1_based > 0 ||
               tilt_coarse_offset_1_based > 0 || zoom_coarse_offset_1_based > 0;
    }
};

bool resolve_fixture_control_offsets(const std::string &gdtf_path,
                                     const std::string &dmx_mode_name,
                                     FixtureControlOffsets &out_offsets,
                                     std::string &out_debug_reason);

FixtureBindingBuildResult build_dimmer_bindings(
    const std::vector<FixturePatch> &patches,
    int universe_offset,
    std::unordered_map<std::string, FixtureControlBinding> &fixture_lookup);

} // namespace peraviz::dmx
