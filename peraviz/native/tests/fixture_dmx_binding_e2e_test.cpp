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

    return 0;
}

} // namespace

int main() {
    return run_test();
}
