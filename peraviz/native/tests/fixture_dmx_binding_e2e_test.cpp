#include "dmx/fixture_dmx_binding.h"
#include "dmx/gdtf_control_offsets_resolver.h"

#include <filesystem>
#include <fstream>
#include <iostream>
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

    return 0;
}

} // namespace

int main() {
    return run_test();
}
