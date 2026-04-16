#include "dmx/fixture_dmx_binding.h"
#include "dmx/gdtf_control_offsets_resolver.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

int fail(const std::string &message) {
    std::cerr << message << std::endl;
    return 1;
}

std::filesystem::path repo_root_from_source() {
    return std::filesystem::weakly_canonical(std::filesystem::path(__FILE__)).parent_path().parent_path().parent_path();
}

std::string read_file(const std::filesystem::path &path) {
    std::ifstream file(path);
    if (!file.good()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool write_gdtf_archive(const std::filesystem::path &path, const std::string &description_xml) {
    wxFileOutputStream file_stream(wxString::FromUTF8(path.string().c_str()));
    if (!file_stream.IsOk()) {
        return false;
    }

    wxZipOutputStream zip(file_stream);
    if (!zip.PutNextEntry("description.xml")) {
        return false;
    }
    zip.Write(description_xml.data(), description_xml.size());
    return zip.Close();
}

const peraviz::dmx::FixtureGoboWheelOffset *find_wheel(
    const peraviz::dmx::FixtureControlOffsets &offsets,
    int wheel_number) {
    for (const peraviz::dmx::FixtureGoboWheelOffset &wheel : offsets.gobo_wheels) {
        if (wheel.wheel_number == wheel_number) {
            return &wheel;
        }
    }
    return nullptr;
}

size_t count_shake_ranges_with_type(
    const peraviz::dmx::FixtureGoboWheelOffset &wheel,
    peraviz::dmx::FixtureGoboShakeControlType control_type) {
    size_t count = 0;
    for (const peraviz::dmx::FixtureGoboShakeRange &range : wheel.shake_ranges) {
        if (range.control_type == control_type) {
            ++count;
        }
    }
    return count;
}



float clamp_unit_interval(float value) {
    return std::max(0.0F, std::min(1.0F, value));
}

float clamp_shake_frequency(float value_hz) {
    return std::max(0.0F, std::min(7.0F, value_hz));
}

float compose_projected_tilt_rotation(float rotation_deg, float shake_tilt_deg) {
    return rotation_deg + shake_tilt_deg;
}

bool should_apply_runtime_shake(bool shake_active,
                                int behavior,
                                bool debug_shake_enabled) {
    constexpr int kGoboBehaviorShake = 3;
    return behavior == kGoboBehaviorShake || shake_active || debug_shake_enabled;
}

const peraviz::dmx::FixtureGoboRange *resolve_active_gobo_range(
    const peraviz::dmx::FixtureGoboWheelOffset &wheel,
    int raw_8bit) {
    const peraviz::dmx::FixtureGoboRange *active_range = nullptr;
    for (const peraviz::dmx::FixtureGoboRange &range : wheel.ranges) {
        int from = range.dmx_from;
        int to = range.dmx_to;
        if (to < from) {
            std::swap(from, to);
        }
        if (raw_8bit < from || raw_8bit > to) {
            continue;
        }
        active_range = &range;
    }
    return active_range;
}

int run_test() {
    const std::filesystem::path repo_root = repo_root_from_source();
    const std::filesystem::path golden_xml_path =
        repo_root / "peraviz/native/tests/data/golden_fixture_description.xml";
    const std::string golden_xml = read_file(golden_xml_path);
    if (golden_xml.empty()) {
        return fail("Failed to read golden GDTF XML fixture description");
    }

    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "peraviz_native_dmx_tests";
    std::error_code ec;
    std::filesystem::create_directories(temp_dir, ec);
    const std::filesystem::path gdtf_path = temp_dir / "golden_fixture.gdtf";
    if (!write_gdtf_archive(gdtf_path, golden_xml)) {
        return fail("Failed to create temporary GDTF archive from golden XML");
    }

    peraviz::dmx::FixtureControlOffsets offsets;
    std::string debug_reason;
    if (!peraviz::dmx::resolve_fixture_control_offsets(gdtf_path.string(), "Standard", offsets, debug_reason)) {
        return fail("resolve_fixture_control_offsets failed: " + debug_reason);
    }

    if (offsets.dimmer_coarse_offset_1_based != 1 || offsets.dimmer_fine_offset_1_based != 2 ||
        offsets.dimmer_ultra_fine_offset_1_based != 3) {
        return fail("Unexpected dimmer offsets");
    }
    if (offsets.pan_coarse_offset_1_based != 4 || offsets.pan_fine_offset_1_based != 5) {
        return fail("Unexpected pan offsets");
    }
    if (offsets.tilt_coarse_offset_1_based != 6 || offsets.tilt_fine_offset_1_based != 7) {
        return fail("Unexpected tilt offsets");
    }
    if (offsets.strobe_coarse_offset_1_based != 9 || offsets.strobe_fine_offset_1_based != 10 ||
        offsets.strobe_ultra_fine_offset_1_based != -1) {
        return fail("Unexpected strobe offsets");
    }
    if (offsets.prism_coarse_offset_1_based != 11 || offsets.prism_fine_offset_1_based != 12) {
        return fail("Unexpected prism offsets");
    }
    if (offsets.prism_index_coarse_offset_1_based != 13 || offsets.prism_index_fine_offset_1_based != 14 ||
        offsets.prism_index_ultra_fine_offset_1_based != 15) {
        return fail("Unexpected prism index offsets");
    }
    if (offsets.color_wheel_coarse_offset_1_based != 18 || offsets.color_wheel_fine_offset_1_based != 19) {
        return fail("Unexpected color wheel offsets");
    }

    if (!offsets.has_zoom_physical_limits || offsets.zoom_physical_min_degrees != 10.0F ||
        offsets.zoom_physical_max_degrees != 40.0F) {
        return fail("Unexpected zoom physical range");
    }

    const peraviz::dmx::FixtureGoboWheelOffset *wheel = find_wheel(offsets, 1);
    if (!wheel) {
        return fail("Expected gobo wheel #1");
    }
    if (wheel->coarse_offset_1_based != 26 || wheel->fine_offset_1_based != 27) {
        return fail("Unexpected gobo wheel select offsets");
    }
    if (!wheel->supports_index || wheel->index_coarse_offset_1_based != 28 || wheel->index_fine_offset_1_based != 29) {
        return fail("Unexpected gobo index channel metadata");
    }
    if (!wheel->supports_rotation || wheel->rotation_coarse_offset_1_based != 30) {
        return fail("Unexpected gobo rotation channel metadata");
    }
    if (wheel->ranges.size() != 4) {
        return fail("Unexpected number of gobo slot ranges");
    }
    if (wheel->ranges[0].behavior != peraviz::dmx::FixtureGoboRangeBehavior::kFixed ||
        wheel->ranges[1].behavior != peraviz::dmx::FixtureGoboRangeBehavior::kIndex ||
        wheel->ranges[2].behavior != peraviz::dmx::FixtureGoboRangeBehavior::kRotation ||
        wheel->ranges[3].behavior != peraviz::dmx::FixtureGoboRangeBehavior::kShake) {
        return fail("Unexpected gobo behavior classification in slot ranges");
    }

    bool found_stop_range = false;
    bool found_cw_range = false;
    bool found_ccw_range = false;
    for (const peraviz::dmx::FixtureGoboRotationRange &range : wheel->rotation_ranges) {
        if (range.is_stop_range && range.dmx_from == 0 && range.dmx_to == 63) {
            found_stop_range = true;
        }
        if (!range.is_stop_range && range.physical_from > 0.0F && range.physical_to > 0.0F) {
            found_cw_range = true;
        }
        if (!range.is_stop_range && range.physical_from < 0.0F && range.physical_to < 0.0F) {
            found_ccw_range = true;
        }
    }
    if (!found_stop_range || !found_cw_range || !found_ccw_range) {
        return fail("Unexpected gobo rotation ranges inferred from channel sets");
    }

    std::unordered_map<std::string, peraviz::dmx::FixtureControlBinding> lookup;
    peraviz::dmx::FixturePatch patch;
    patch.fixture_uuid = "fixture-1";
    patch.mvr_universe = 2;
    patch.mvr_address = 100;
    patch.dmx_mode = "Standard";
    patch.gdtf_path = gdtf_path.string();
    const std::vector<peraviz::dmx::FixturePatch> patches = {patch};

    const peraviz::dmx::FixtureBindingBuildResult result =
        peraviz::dmx::build_fixture_control_bindings(patches, 10, lookup);

    if (!result.unbound.empty()) {
        return fail("Expected fixture patch to bind successfully");
    }
    if (result.bindings.size() != 1 || lookup.size() != 1) {
        return fail("Unexpected fixture binding count");
    }

    const peraviz::dmx::FixtureControlBinding &binding = result.bindings.front();
    if (binding.artnet_universe_id != 12) {
        return fail("Unexpected Art-Net universe mapping");
    }
    if (binding.dimmer.coarse_dmx_channel_index_0 != 99 || binding.dimmer.fine_dmx_channel_index_0 != 100 ||
        binding.dimmer.ultra_fine_dmx_channel_index_0 != 101) {
        return fail("Unexpected dimmer channel index mapping");
    }
    if (binding.gobo.coarse_dmx_channel_index_0 != 124 || binding.gobo.fine_dmx_channel_index_0 != 125) {
        return fail("Unexpected gobo channel index mapping");
    }
    if (binding.gobo_rotation.coarse_dmx_channel_index_0 != 128) {
        return fail("Unexpected gobo rotation channel index mapping");
    }
    if (binding.strobe.ultra_fine_dmx_channel_index_0 != -1 || binding.prism.ultra_fine_dmx_channel_index_0 != -1) {
        return fail("Expected fine-only channels to keep ultra-fine disabled");
    }

    const std::string mega_pointe_like_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<GDTF>
  <AttributeDefinitions>
    <Attributes>
      <Attribute Name="Gobo2SelectShake">
        <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Angle" PhysicalFrom="2.0" PhysicalTo="2.0" />
      </Attribute>
      <Attribute Name="Gobo2ShakeIndex">
        <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Percent" PhysicalFrom="1.0" PhysicalTo="2.0" />
      </Attribute>
    </Attributes>
  </AttributeDefinitions>
  <Wheels>
    <Wheel Name="Gobo2">
      <Slot WheelSlotIndex="1" />
      <Slot WheelSlotIndex="2" />
    </Wheel>
  </Wheels>
  <DMXModes>
    <DMXMode Name="WithDedicatedShake">
      <DMXChannels>
        <DMXChannel Offset="1">
          <LogicalChannel Attribute="Gobo2SelectShake">
            <ChannelFunction Attribute="Gobo2SelectShake" Name="Gobo2SelectShake" Wheel="gobo2" DMXFrom="0" DMXTo="127">
              <ChannelSet Name="Slot 1" WheelSlotIndex="1" DMXFrom="0" DMXTo="63" PhysicalFrom="1" PhysicalTo="4" />
              <ChannelSet Name="Slot 2" WheelSlotIndex="2" DMXFrom="64" DMXTo="127" PhysicalFrom="5" PhysicalTo="9" />
            </ChannelFunction>
          </LogicalChannel>
        </DMXChannel>
        <DMXChannel Offset="2">
          <LogicalChannel Attribute="StaticGoboShake">
            <ChannelFunction Attribute="Gobo2ShakeIndex" Name="StaticGoboShake" Wheel="gobo2" DMXFrom="0" DMXTo="255">
              <ChannelSet Name="Shake Slow to Fast" DMXFrom="0" DMXTo="127" PhysicalFrom="10" PhysicalTo="35" />
              <ChannelSet Name="Shake Fast to Slow" DMXFrom="128" DMXTo="255" PhysicalFrom="35" PhysicalTo="10" />
            </ChannelFunction>
          </LogicalChannel>
        </DMXChannel>
      </DMXChannels>
    </DMXMode>
    <DMXMode Name="WithoutDedicatedShake">
      <DMXChannels>
        <DMXChannel Offset="1">
          <LogicalChannel Attribute="Gobo2SelectShake">
            <ChannelFunction Attribute="Gobo2SelectShake" Name="Gobo2SelectShake" Wheel="gobo2" DMXFrom="0" DMXTo="127">
              <ChannelSet Name="Slot 1" WheelSlotIndex="1" DMXFrom="0" DMXTo="63" PhysicalFrom="1" PhysicalTo="4" />
              <ChannelSet Name="Slot 2" WheelSlotIndex="2" DMXFrom="64" DMXTo="127" PhysicalFrom="5" PhysicalTo="9" />
            </ChannelFunction>
          </LogicalChannel>
        </DMXChannel>
      </DMXChannels>
    </DMXMode>
  </DMXModes>
</GDTF>)";

    const std::filesystem::path mega_pointe_gdtf_path = temp_dir / "mega_pointe_like_fixture.gdtf";
    if (!write_gdtf_archive(mega_pointe_gdtf_path, mega_pointe_like_xml)) {
        return fail("Failed to create MegaPointe-like GDTF archive");
    }

    peraviz::dmx::FixtureControlOffsets with_dedicated_offsets;
    if (!peraviz::dmx::resolve_fixture_control_offsets(mega_pointe_gdtf_path.string(),
                                                       "WithDedicatedShake",
                                                       with_dedicated_offsets,
                                                       debug_reason)) {
        return fail("resolve_fixture_control_offsets failed for dedicated shake mode: " + debug_reason);
    }
    const peraviz::dmx::FixtureGoboWheelOffset *with_dedicated_wheel = find_wheel(with_dedicated_offsets, 2);
    if (!with_dedicated_wheel) {
        return fail("Expected gobo wheel #2 in dedicated shake mode");
    }
    if (with_dedicated_wheel->coarse_offset_1_based != 1) {
        return fail("Expected select channel to keep gobo slot ownership in dedicated shake mode");
    }
    if (with_dedicated_wheel->rotation_coarse_offset_1_based != 2) {
        return fail("Expected dedicated shake channel to own shake speed offset");
    }
    if (with_dedicated_wheel->ranges.size() != 2 ||
        with_dedicated_wheel->ranges[0].slot_index != 1 ||
        with_dedicated_wheel->ranges[1].slot_index != 2) {
        return fail("Expected slot ranges to remain intact when dedicated shake exists");
    }
    if (count_shake_ranges_with_type(*with_dedicated_wheel,
                                     peraviz::dmx::FixtureGoboShakeControlType::kDedicatedShakeChannel) == 0) {
        return fail("Expected dedicated shake ranges in dedicated shake mode");
    }
    if (count_shake_ranges_with_type(*with_dedicated_wheel,
                                     peraviz::dmx::FixtureGoboShakeControlType::kSameChannelSelect) == 0) {
        return fail("Expected select-channel shake fallback ranges in dedicated shake mode");
    }
    bool has_slot_bound_select_shake = false;
    bool has_amplitude_from_attribute = false;
    for (const peraviz::dmx::FixtureGoboShakeRange &range : with_dedicated_wheel->shake_ranges) {
        if (range.control_type == peraviz::dmx::FixtureGoboShakeControlType::kSameChannelSelect &&
            range.slot_index > 0 &&
            range.physical_to > range.physical_from) {
            has_slot_bound_select_shake = true;
        }
        if (range.has_explicit_amplitude &&
            range.amplitude_from_degrees > 3.5F &&
            range.amplitude_to_degrees > range.amplitude_from_degrees) {
            has_amplitude_from_attribute = true;
        }
    }
    if (!has_slot_bound_select_shake) {
        return fail("Expected slot-bound select-shake ranges with physical windows");
    }
    if (!has_amplitude_from_attribute) {
        return fail("Expected shake ranges to include amplitude from SubPhysicalUnit");
    }

    peraviz::dmx::FixtureControlOffsets without_dedicated_offsets;
    if (!peraviz::dmx::resolve_fixture_control_offsets(mega_pointe_gdtf_path.string(),
                                                       "WithoutDedicatedShake",
                                                       without_dedicated_offsets,
                                                       debug_reason)) {
        return fail("resolve_fixture_control_offsets failed for fallback shake mode: " + debug_reason);
    }
    const peraviz::dmx::FixtureGoboWheelOffset *without_dedicated_wheel = find_wheel(without_dedicated_offsets, 2);
    if (!without_dedicated_wheel) {
        return fail("Expected gobo wheel #2 in fallback shake mode");
    }
    if (without_dedicated_wheel->rotation_coarse_offset_1_based > 0) {
        return fail("Did not expect dedicated shake offset in fallback shake mode");
    }
    if (count_shake_ranges_with_type(*without_dedicated_wheel,
                                     peraviz::dmx::FixtureGoboShakeControlType::kDedicatedShakeChannel) != 0) {
        return fail("Did not expect dedicated shake ranges in fallback shake mode");
    }
    if (count_shake_ranges_with_type(*without_dedicated_wheel,
                                     peraviz::dmx::FixtureGoboShakeControlType::kSameChannelSelect) == 0) {
        return fail("Expected fallback shake ranges sourced from select channel");
    }

    const std::string select_shake_static_like_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<GDTF>
  <Wheels>
    <Wheel Name="Gobo1">
      <Slot WheelSlotIndex="1" />
      <Slot WheelSlotIndex="2" />
    </Wheel>
  </Wheels>
  <DMXModes>
    <DMXMode Name="SelectShakeStaticLike">
      <DMXChannels>
        <DMXChannel Offset="1">
          <LogicalChannel Attribute="StaticGoboShake">
            <ChannelFunction Attribute="Gobo1SelectShake" Name="Gobo1ShakeIndex" Wheel="gobo1" DMXFrom="0" DMXTo="255">
              <ChannelSet Name="Gobo 1" WheelSlotIndex="1" DMXFrom="0" DMXTo="127" />
              <ChannelSet Name="Gobo 2" WheelSlotIndex="2" DMXFrom="128" DMXTo="255" />
            </ChannelFunction>
          </LogicalChannel>
        </DMXChannel>
      </DMXChannels>
    </DMXMode>
  </DMXModes>
</GDTF>)";

    const std::filesystem::path select_shake_static_like_gdtf_path =
        temp_dir / "select_shake_static_like_fixture.gdtf";
    if (!write_gdtf_archive(select_shake_static_like_gdtf_path,
                            select_shake_static_like_xml)) {
        return fail("Failed to create select-shake static-like GDTF archive");
    }

    peraviz::dmx::FixtureControlOffsets select_shake_static_like_offsets;
    if (!peraviz::dmx::resolve_fixture_control_offsets(
            select_shake_static_like_gdtf_path.string(),
            "SelectShakeStaticLike",
            select_shake_static_like_offsets,
            debug_reason)) {
        return fail("resolve_fixture_control_offsets failed for select-shake static-like mode: " +
                    debug_reason);
    }
    const peraviz::dmx::FixtureGoboWheelOffset *select_shake_static_like_wheel =
        find_wheel(select_shake_static_like_offsets, 1);
    if (!select_shake_static_like_wheel) {
        return fail("Expected gobo wheel #1 in select-shake static-like mode");
    }
    if (select_shake_static_like_wheel->ranges.size() != 2) {
        return fail("Expected two gobo ranges in select-shake static-like mode");
    }
    bool has_fixed_behavior = false;
    for (const peraviz::dmx::FixtureGoboRange &range :
         select_shake_static_like_wheel->ranges) {
        if (range.behavior == peraviz::dmx::FixtureGoboRangeBehavior::kFixed) {
            has_fixed_behavior = true;
        }
        if (range.behavior != peraviz::dmx::FixtureGoboRangeBehavior::kShake) {
            return fail("Expected select-shake static-like ranges to force shake behavior");
        }
    }
    if (has_fixed_behavior) {
        return fail("Did not expect select-shake static-like ranges to degrade to fixed behavior");
    }

    const std::string dual_select_shake_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<GDTF>
  <AttributeDefinitions>
    <Attributes>
      <Attribute Name="Gobo1SelectShake">
        <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Angle" PhysicalFrom="0.3" PhysicalTo="1.4" />
      </Attribute>
      <Attribute Name="Gobo2SelectShake">
        <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Angle" PhysicalFrom="0.2" PhysicalTo="1.8" />
      </Attribute>
    </Attributes>
  </AttributeDefinitions>
  <Wheels>
    <Wheel Name="Gobo1"><Slot WheelSlotIndex="1"/></Wheel>
    <Wheel Name="Gobo2"><Slot WheelSlotIndex="1"/></Wheel>
  </Wheels>
  <DMXModes>
    <DMXMode Name="DualSelectShake">
      <DMXChannels>
        <DMXChannel Offset="1">
          <LogicalChannel Attribute="Gobo1SelectShake">
            <ChannelFunction Attribute="Gobo1SelectShake" Name="Gobo1SelectShake" Wheel="gobo1" DMXFrom="0" DMXTo="127">
              <ChannelSet Name="G1 Shake" WheelSlotIndex="1" DMXFrom="0" DMXTo="127" PhysicalFrom="0.5" PhysicalTo="4.0"/>
            </ChannelFunction>
          </LogicalChannel>
        </DMXChannel>
        <DMXChannel Offset="2">
          <LogicalChannel Attribute="Gobo2SelectShake">
            <ChannelFunction Attribute="Gobo2SelectShake" Name="Gobo2SelectShake" Wheel="gobo2" DMXFrom="0" DMXTo="127">
              <ChannelSet Name="G2 Shake" WheelSlotIndex="1" DMXFrom="0" DMXTo="127" PhysicalFrom="1.0" PhysicalTo="5.0"/>
            </ChannelFunction>
          </LogicalChannel>
        </DMXChannel>
      </DMXChannels>
    </DMXMode>
  </DMXModes>
</GDTF>)";

    const std::filesystem::path dual_select_shake_path =
        temp_dir / "dual_select_shake_fixture.gdtf";
    if (!write_gdtf_archive(dual_select_shake_path, dual_select_shake_xml)) {
        return fail("Failed to create dual-select-shake GDTF archive");
    }

    peraviz::dmx::FixtureControlOffsets dual_select_shake_offsets;
    if (!peraviz::dmx::resolve_fixture_control_offsets(
            dual_select_shake_path.string(),
            "DualSelectShake",
            dual_select_shake_offsets,
            debug_reason)) {
        return fail("resolve_fixture_control_offsets failed for dual-select-shake mode: " +
                    debug_reason);
    }

    const peraviz::dmx::FixtureGoboWheelOffset *dual_gobo1 =
        find_wheel(dual_select_shake_offsets, 1);
    const peraviz::dmx::FixtureGoboWheelOffset *dual_gobo2 =
        find_wheel(dual_select_shake_offsets, 2);
    if (!dual_gobo1 || !dual_gobo2) {
        return fail("Expected both gobo wheels in dual-select-shake mode");
    }
    if (dual_gobo1->shake_ranges.empty() || dual_gobo2->shake_ranges.empty()) {
        return fail("Expected shake_ranges extraction for Gobo1SelectShake and Gobo2SelectShake");
    }

    bool gobo1_has_explicit_amplitude = false;
    bool gobo2_has_explicit_amplitude = false;
    for (const peraviz::dmx::FixtureGoboShakeRange &range : dual_gobo1->shake_ranges) {
        if (range.has_explicit_amplitude && range.amplitude_to_degrees > range.amplitude_from_degrees) {
            gobo1_has_explicit_amplitude = true;
        }
    }
    for (const peraviz::dmx::FixtureGoboShakeRange &range : dual_gobo2->shake_ranges) {
        if (range.has_explicit_amplitude && range.amplitude_to_degrees > range.amplitude_from_degrees) {
            gobo2_has_explicit_amplitude = true;
        }
    }
    if (!gobo1_has_explicit_amplitude || !gobo2_has_explicit_amplitude) {
        return fail("Expected explicit shake amplitude extraction from SubPhysicalUnit Type='Amplitude'");
    }

    const bool runtime_shake_active = true;
    const bool debug_override_shake_enabled = false;
    if (!should_apply_runtime_shake(runtime_shake_active, 2, debug_override_shake_enabled)) {
        return fail("Expected DMX shake to remain active when debug override is disabled");
    }

    const float composed_final_tilt_deg = compose_projected_tilt_rotation(15.0F, 0.35F);
    if (std::abs(composed_final_tilt_deg - 15.35F) > 0.0001F) {
        return fail("Expected runtime composition final = rotation + shake_tilt");
    }

    if (std::abs(clamp_unit_interval(1.9F) - 1.0F) > 0.0001F) {
        return fail("Expected shake amplitude clamp upper bound to 1.0");
    }
    if (std::abs(clamp_shake_frequency(9.4F) - 7.0F) > 0.0001F) {
        return fail("Expected shake frequency clamp upper bound to 7.0");
    }

    const std::filesystem::path canonical_mega_pointe_path =
        repo_root / "library/fixtures/Robin MegaPointe.gdtf";
    if (!std::filesystem::exists(canonical_mega_pointe_path)) {
        return fail("Expected canonical fixture at library/fixtures/Robin MegaPointe.gdtf");
    }

    peraviz::dmx::FixtureControlOffsets mega_pointe_offsets;
    if (!peraviz::dmx::resolve_fixture_control_offsets(canonical_mega_pointe_path.string(),
                                                       "Mode 1 - Standard 16 - bit",
                                                       mega_pointe_offsets,
                                                       debug_reason)) {
        return fail("resolve_fixture_control_offsets failed for canonical MegaPointe fixture: " + debug_reason);
    }

    const peraviz::dmx::FixtureGoboWheelOffset *mega_pointe_gobo1 =
        find_wheel(mega_pointe_offsets, 1);
    if (!mega_pointe_gobo1) {
        return fail("Expected gobo wheel #1 in canonical MegaPointe fixture");
    }
    if (mega_pointe_gobo1->shake_ranges.empty()) {
        return fail("Expected canonical MegaPointe gobo1 shake ranges to be non-empty");
    }
    for (const peraviz::dmx::FixtureGoboShakeRange &shake_range : mega_pointe_gobo1->shake_ranges) {
        if (shake_range.control_type != peraviz::dmx::FixtureGoboShakeControlType::kSameChannelSelect) {
            return fail("Expected canonical MegaPointe gobo1 shake ranges to use same-channel control type");
        }
    }

    const std::array<int, 5> gobo1_shake_probe_values = {88, 96, 128, 160, 201};
    for (const int raw_8bit : gobo1_shake_probe_values) {
        const peraviz::dmx::FixtureGoboRange *active_range =
            resolve_active_gobo_range(*mega_pointe_gobo1, raw_8bit);
        if (!active_range) {
            return fail("Expected active gobo1 range for canonical MegaPointe shake probe DMX value");
        }
        if (active_range->slot_index <= 0) {
            return fail("Expected active gobo1 range for canonical MegaPointe shake probe DMX value to keep slot selection");
        }

        bool found_matching_select_shake_range = false;
        for (const peraviz::dmx::FixtureGoboShakeRange &shake_range : mega_pointe_gobo1->shake_ranges) {
            if (shake_range.control_type != peraviz::dmx::FixtureGoboShakeControlType::kSameChannelSelect) {
                continue;
            }
            int shake_from = shake_range.dmx_from;
            int shake_to = shake_range.dmx_to;
            if (shake_to < shake_from) {
                std::swap(shake_from, shake_to);
            }
            if (shake_range.slot_index == active_range->slot_index &&
                raw_8bit >= shake_from &&
                raw_8bit <= shake_to) {
                found_matching_select_shake_range = true;
                break;
            }
        }
        if (!found_matching_select_shake_range) {
            return fail("Expected canonical MegaPointe gobo1 shake probe DMX value to have matching same-channel shake range");
        }
    }

    const peraviz::dmx::FixtureGoboWheelOffset *mega_pointe_gobo2 =
        find_wheel(mega_pointe_offsets, 2);
    if (!mega_pointe_gobo2) {
        return fail("Expected gobo wheel #2 in canonical MegaPointe fixture");
    }
    if (mega_pointe_gobo2->shake_ranges.empty()) {
        return fail("Expected canonical MegaPointe gobo2 shake ranges to be non-empty");
    }
    for (const peraviz::dmx::FixtureGoboShakeRange &shake_range : mega_pointe_gobo2->shake_ranges) {
        if (shake_range.control_type != peraviz::dmx::FixtureGoboShakeControlType::kSameChannelSelect) {
            return fail("Expected canonical MegaPointe gobo2 shake ranges to use same-channel control type");
        }
    }

    return 0;
}

} // namespace

int main() {
    return run_test();
}
