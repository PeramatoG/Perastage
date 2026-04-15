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

struct FixtureGoboSlot {
    int slot_index = -1;
    std::string image_path;
};

enum class FixtureGoboRangeBehavior {
    kFixed = 0,
    kIndex = 1,
    kRotation = 2,
    kShake = 3,
};

struct FixtureGoboRange {
    int dmx_from = 0;
    int dmx_to = 255;
    int mode_from_8bit = 0;
    int mode_to_8bit = 255;
    int slot_index = -1;
    FixtureGoboRangeBehavior behavior = FixtureGoboRangeBehavior::kFixed;
};

struct FixtureGoboRotationRange {
    int dmx_from = 0;
    int dmx_to = 255;
    int mode_from_8bit = 0;
    int mode_to_8bit = 255;
    float physical_from = 0.0F;
    float physical_to = 0.0F;
    bool is_stop_range = false;
    bool is_rotation_channel_range = false;
};

enum class FixtureGoboShakeControlType {
    kSameChannelSelect = 0,
    kDedicatedShakeChannel = 1,
};

struct FixtureGoboShakeRange {
    int dmx_from = 0;
    int dmx_to = 255;
    int mode_from_8bit = 0;
    int mode_to_8bit = 255;
    float physical_from = 0.0F;
    float physical_to = 0.0F;
    bool has_explicit_amplitude = false;
    float amplitude_from_degrees = 0.0F;
    float amplitude_to_degrees = 0.0F;
    int slot_index = -1;
    FixtureGoboShakeControlType control_type = FixtureGoboShakeControlType::kSameChannelSelect;
};

struct FixtureGoboWheelBinding {
    FixtureAttributeChannel channel;
    FixtureAttributeChannel index_channel;
    FixtureAttributeChannel rotation_channel;
    bool supports_index = false;
    bool supports_rotation = false;
    bool supports_spin_rotation = false;
    bool supports_shake = false;
    bool has_index_physical_limits = false;
    float index_physical_min = 0.0F;
    float index_physical_max = 0.0F;
    std::vector<FixtureGoboRotationRange> rotation_ranges;
    std::vector<FixtureGoboShakeRange> shake_ranges;
    int wheel_number = 0;
    std::string wheel_name;
    std::vector<FixtureGoboSlot> slots;
    std::vector<FixtureGoboRange> ranges;
};

struct FixtureControlBinding {
    std::string fixture_uuid;
    int artnet_universe_id = -1;
    FixtureAttributeChannel dimmer;
    FixtureAttributeChannel pan;
    FixtureAttributeChannel tilt;
    FixtureAttributeChannel zoom;
    FixtureAttributeChannel cyan;
    FixtureAttributeChannel magenta;
    FixtureAttributeChannel yellow;
    FixtureAttributeChannel strobe;
    FixtureAttributeChannel prism;
    FixtureAttributeChannel prism_index;
    FixtureAttributeChannel prism_rotation;
    FixtureAttributeChannel color_wheel;
    FixtureAttributeChannel color_wheel_rotation;
    FixtureAttributeChannel animation_wheel;
    FixtureAttributeChannel animation_wheel_rotation;
    FixtureAttributeChannel gobo;
    FixtureAttributeChannel gobo_index;
    FixtureAttributeChannel gobo_rotation;
    int gobo_wheel_number = 0;
    std::string gobo_wheel_name;
    std::vector<FixtureGoboSlot> gobo_slots;
    std::vector<FixtureGoboRange> gobo_ranges;
    std::vector<FixtureGoboWheelBinding> gobo_wheels;
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

struct FixtureGoboWheelOffset {
    int coarse_offset_1_based = -1;
    int fine_offset_1_based = -1;
    int ultra_fine_offset_1_based = -1;
    int index_coarse_offset_1_based = -1;
    int index_fine_offset_1_based = -1;
    int index_ultra_fine_offset_1_based = -1;
    int rotation_coarse_offset_1_based = -1;
    int rotation_fine_offset_1_based = -1;
    int rotation_ultra_fine_offset_1_based = -1;
    int rotation_channel_priority = -1;
    bool supports_index = false;
    bool supports_rotation = false;
    bool supports_spin_rotation = false;
    bool supports_shake = false;
    bool has_index_physical_limits = false;
    float index_physical_min = 0.0F;
    float index_physical_max = 0.0F;
    std::vector<FixtureGoboRotationRange> rotation_ranges;
    std::vector<FixtureGoboShakeRange> shake_ranges;
    int wheel_number = 0;
    std::string wheel_name;
    std::vector<FixtureGoboSlot> slots;
    std::vector<FixtureGoboRange> ranges;
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
    int cyan_coarse_offset_1_based = -1;
    int cyan_fine_offset_1_based = -1;
    int cyan_ultra_fine_offset_1_based = -1;
    int magenta_coarse_offset_1_based = -1;
    int magenta_fine_offset_1_based = -1;
    int magenta_ultra_fine_offset_1_based = -1;
    int yellow_coarse_offset_1_based = -1;
    int yellow_fine_offset_1_based = -1;
    int yellow_ultra_fine_offset_1_based = -1;
    int strobe_coarse_offset_1_based = -1;
    int strobe_fine_offset_1_based = -1;
    int strobe_ultra_fine_offset_1_based = -1;
    int prism_coarse_offset_1_based = -1;
    int prism_fine_offset_1_based = -1;
    int prism_ultra_fine_offset_1_based = -1;
    int prism_index_coarse_offset_1_based = -1;
    int prism_index_fine_offset_1_based = -1;
    int prism_index_ultra_fine_offset_1_based = -1;
    int prism_rotation_coarse_offset_1_based = -1;
    int prism_rotation_fine_offset_1_based = -1;
    int prism_rotation_ultra_fine_offset_1_based = -1;
    int color_wheel_coarse_offset_1_based = -1;
    int color_wheel_fine_offset_1_based = -1;
    int color_wheel_ultra_fine_offset_1_based = -1;
    int color_wheel_rotation_coarse_offset_1_based = -1;
    int color_wheel_rotation_fine_offset_1_based = -1;
    int color_wheel_rotation_ultra_fine_offset_1_based = -1;
    int animation_wheel_coarse_offset_1_based = -1;
    int animation_wheel_fine_offset_1_based = -1;
    int animation_wheel_ultra_fine_offset_1_based = -1;
    int animation_wheel_rotation_coarse_offset_1_based = -1;
    int animation_wheel_rotation_fine_offset_1_based = -1;
    int animation_wheel_rotation_ultra_fine_offset_1_based = -1;
    int gobo_coarse_offset_1_based = -1;
    int gobo_fine_offset_1_based = -1;
    int gobo_ultra_fine_offset_1_based = -1;
    int gobo_index_coarse_offset_1_based = -1;
    int gobo_index_fine_offset_1_based = -1;
    int gobo_index_ultra_fine_offset_1_based = -1;
    int gobo_rotation_coarse_offset_1_based = -1;
    int gobo_rotation_fine_offset_1_based = -1;
    int gobo_rotation_ultra_fine_offset_1_based = -1;
    int gobo_wheel_number = 0;
    std::string gobo_wheel_name;
    std::vector<FixtureGoboSlot> gobo_slots;
    std::vector<FixtureGoboRange> gobo_ranges;
    std::vector<FixtureGoboWheelOffset> gobo_wheels;
    bool has_zoom_physical_limits = false;
    float zoom_physical_min_degrees = -1.0F;
    float zoom_physical_max_degrees = -1.0F;

    bool has_any() const {
        return dimmer_coarse_offset_1_based > 0 || pan_coarse_offset_1_based > 0 ||
               tilt_coarse_offset_1_based > 0 || zoom_coarse_offset_1_based > 0 ||
               cyan_coarse_offset_1_based > 0 || magenta_coarse_offset_1_based > 0 ||
               strobe_coarse_offset_1_based > 0 || prism_coarse_offset_1_based > 0 ||
               prism_index_coarse_offset_1_based > 0 || prism_rotation_coarse_offset_1_based > 0 ||
               color_wheel_coarse_offset_1_based > 0 || color_wheel_rotation_coarse_offset_1_based > 0 ||
               animation_wheel_coarse_offset_1_based > 0 ||
               animation_wheel_rotation_coarse_offset_1_based > 0 ||
               gobo_coarse_offset_1_based > 0 || gobo_index_coarse_offset_1_based > 0 ||
               gobo_rotation_coarse_offset_1_based > 0 || yellow_coarse_offset_1_based > 0 ||
               !gobo_wheels.empty();
    }
};

bool resolve_fixture_control_offsets(const std::string &gdtf_path,
                                     const std::string &dmx_mode_name,
                                     FixtureControlOffsets &out_offsets,
                                     std::string &out_debug_reason);

FixtureBindingBuildResult build_fixture_control_bindings(
    const std::vector<FixturePatch> &patches,
    int universe_offset,
    std::unordered_map<std::string, FixtureControlBinding> &fixture_lookup);

// Deprecated alias kept for temporary backward compatibility.
FixtureBindingBuildResult build_dimmer_bindings(
    const std::vector<FixturePatch> &patches,
    int universe_offset,
    std::unordered_map<std::string, FixtureControlBinding> &fixture_lookup);

} // namespace peraviz::dmx
