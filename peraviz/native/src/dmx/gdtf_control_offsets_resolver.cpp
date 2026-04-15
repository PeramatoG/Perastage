#include "dmx/fixture_dmx_binding.h"

#include "dmx/gdtf_attribute_classifier.h"
#include "dmx/gdtf_gobo_catalog.h"
#include "dmx/gdtf_physical_ranges.h"
#include "dmx/gdtf_xml_reader.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <tinyxml2.h>

namespace {

struct DimmerResolveCacheEntry {
    bool ok = false;
    peraviz::dmx::FixtureControlOffsets offsets;
    std::string reason;
};

std::mutex g_cache_mutex;
std::unordered_map<std::string, DimmerResolveCacheEntry> g_cache;

std::string make_cache_key(const std::string &gdtf_path, const std::string &dmx_mode_name) {
    return gdtf_path + "\n" + dmx_mode_name;
}

void consume_offsets(const std::vector<int> &offsets,
                     bool is_fine,
                     int byte_index,
                     int &coarse,
                     int &fine,
                     int &ultra_fine) {
    if (offsets.empty()) {
        return;
    }

    if (is_fine) {
        int fine_offset_index = 0;
        if (byte_index >= 2 && static_cast<size_t>(byte_index - 1) < offsets.size()) {
            fine_offset_index = byte_index - 1;
        } else if (offsets.size() > 1) {
            fine_offset_index = 1;
        }

        const int fine_candidate = offsets[static_cast<size_t>(fine_offset_index)];
        if (fine <= 0 || fine_candidate < fine) {
            fine = fine_candidate;
        }

        const size_t ultra_index = static_cast<size_t>(fine_offset_index + 1);
        if (ultra_index < offsets.size() &&
            (ultra_fine <= 0 || offsets[ultra_index] < ultra_fine)) {
            ultra_fine = offsets[ultra_index];
        }
        return;
    }

    if (coarse <= 0 || offsets[0] < coarse) {
        coarse = offsets[0];
    }
    if (offsets.size() > 1 && (fine <= 0 || offsets[1] < fine)) {
        fine = offsets[1];
    }
    if (offsets.size() > 2 && (ultra_fine <= 0 || offsets[2] < ultra_fine)) {
        ultra_fine = offsets[2];
    }
}

int parse_last_number_token(const std::string &text) {
    int value = 0;
    bool found = false;
    int current = 0;
    bool in_digits = false;
    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            in_digits = true;
            current = (current * 10) + (ch - '0');
            continue;
        }
        if (in_digits) {
            value = current;
            found = true;
            current = 0;
            in_digits = false;
        }
    }
    if (in_digits) {
        value = current;
        found = true;
    }
    return found ? value : 0;
}

peraviz::dmx::FixtureGoboWheelOffset *find_or_create_gobo_wheel_offset(
    peraviz::dmx::FixtureControlOffsets &out_offsets,
    int wheel_number,
    const std::string &wheel_name) {
    for (auto &wheel : out_offsets.gobo_wheels) {
        if (!wheel_name.empty() && !wheel.wheel_name.empty() && wheel.wheel_name == wheel_name) {
            if (wheel.wheel_number <= 0 && wheel_number > 0) {
                wheel.wheel_number = wheel_number;
            }
            return &wheel;
        }
        if (wheel_number > 0 && wheel.wheel_number == wheel_number) {
            if (wheel.wheel_name.empty() && !wheel_name.empty()) {
                wheel.wheel_name = wheel_name;
            }
            return &wheel;
        }
    }

    peraviz::dmx::FixtureGoboWheelOffset wheel;
    wheel.wheel_number = wheel_number;
    wheel.wheel_name = wheel_name;
    out_offsets.gobo_wheels.push_back(std::move(wheel));
    return &out_offsets.gobo_wheels.back();
}

void consume_channel_offsets(tinyxml2::XMLElement *dmx_channel,
                             const peraviz::dmx::GoboWheelCatalog &wheel_catalog,
                             peraviz::dmx::FixtureControlOffsets &out_offsets) {
    if (!dmx_channel) {
        return;
    }

    const std::vector<int> offsets = peraviz::dmx::parse_offsets(dmx_channel->Attribute("Offset"));
    if (offsets.empty()) {
        return;
    }

    const std::vector<tinyxml2::XMLElement *> logical_channels =
        peraviz::dmx::collect_direct_children_by_name(dmx_channel, "logicalchannel");
    for (tinyxml2::XMLElement *logical_channel : logical_channels) {
        const char *logical_attribute = logical_channel->Attribute("Attribute");
        if (!logical_attribute) {
            logical_attribute = logical_channel->Attribute("attribute");
        }

        const std::vector<tinyxml2::XMLElement *> channel_functions =
            peraviz::dmx::collect_direct_children_by_name(logical_channel, "channelfunction");
        std::vector<int> function_dmx_from_values;
        std::vector<int> function_dmx_to_values;
        function_dmx_from_values.reserve(channel_functions.size());
        function_dmx_to_values.reserve(channel_functions.size());
        std::vector<int> function_mode_from_values;
        std::vector<int> function_mode_to_values;
        function_mode_from_values.reserve(channel_functions.size());
        function_mode_to_values.reserve(channel_functions.size());
        for (tinyxml2::XMLElement *channel_function : channel_functions) {
            int function_dmx_from = peraviz::dmx::parse_dmx_value_8bit(channel_function->Attribute("DMXFrom"));
            if (function_dmx_from < 0) {
                function_dmx_from = peraviz::dmx::parse_dmx_value_8bit(channel_function->Attribute("dmxfrom"));
            }
            if (function_dmx_from < 0) {
                function_dmx_from = 0;
            }

            int function_dmx_to = peraviz::dmx::parse_dmx_value_8bit(channel_function->Attribute("DMXTo"));
            if (function_dmx_to < 0) {
                function_dmx_to = peraviz::dmx::parse_dmx_value_8bit(channel_function->Attribute("dmxto"));
            }

            const bool has_mode_master =
                channel_function->Attribute("ModeMaster") != nullptr ||
                channel_function->Attribute("modemaster") != nullptr;

            int parsed_mode_from = peraviz::dmx::parse_dmx_value_8bit(channel_function->Attribute("ModeFrom"));
            if (parsed_mode_from < 0) {
                parsed_mode_from = peraviz::dmx::parse_dmx_value_8bit(channel_function->Attribute("modefrom"));
            }

            int parsed_mode_to = peraviz::dmx::parse_dmx_value_8bit(channel_function->Attribute("ModeTo"));
            if (parsed_mode_to < 0) {
                parsed_mode_to = peraviz::dmx::parse_dmx_value_8bit(channel_function->Attribute("modeto"));
            }

            int function_mode_from = 0;
            int function_mode_to = 255;
            if (has_mode_master) {
                function_mode_from = parsed_mode_from >= 0 ? parsed_mode_from : 0;
                function_mode_to = parsed_mode_to >= 0 ? parsed_mode_to : 255;
            }

            function_dmx_from = std::clamp(function_dmx_from, 0, 255);
            if (function_dmx_to >= 0) {
                function_dmx_to = std::clamp(function_dmx_to, 0, 255);
                if (function_dmx_to < function_dmx_from) {
                    std::swap(function_dmx_to, function_dmx_from);
                }
            }

            function_mode_from = std::clamp(function_mode_from, 0, 255);
            function_mode_to = std::clamp(function_mode_to, 0, 255);
            if (function_mode_to < function_mode_from) {
                std::swap(function_mode_to, function_mode_from);
            }

            function_dmx_from_values.push_back(function_dmx_from);
            function_dmx_to_values.push_back(function_dmx_to);
            function_mode_from_values.push_back(function_mode_from);
            function_mode_to_values.push_back(function_mode_to);
        }

        for (size_t channel_function_index = 0;
             channel_function_index < channel_functions.size();
             ++channel_function_index) {
            tinyxml2::XMLElement *channel_function = channel_functions[channel_function_index];
            const int function_dmx_from = function_dmx_from_values[channel_function_index];
            int function_dmx_to = function_dmx_to_values[channel_function_index];
            if (function_dmx_to < 0) {
                function_dmx_to = 255;
                if (channel_function_index + 1 < channel_functions.size()) {
                    const int next_function_dmx_from = function_dmx_from_values[channel_function_index + 1];
                    if (next_function_dmx_from > function_dmx_from) {
                        function_dmx_to = next_function_dmx_from - 1;
                    }
                }
            }
            function_dmx_to = std::clamp(function_dmx_to, function_dmx_from, 255);
            const int function_mode_from = function_mode_from_values[channel_function_index];
            const int function_mode_to = function_mode_to_values[channel_function_index];
            const char *attribute = channel_function->Attribute("Attribute");
            if (!attribute) {
                attribute = channel_function->Attribute("attribute");
            }
            if (!attribute) {
                attribute = logical_attribute;
            }
            if (!attribute) {
                continue;
            }

            const peraviz::dmx::ParsedAttribute parsed_attribute = peraviz::dmx::parse_attribute_name(attribute);
            switch (parsed_attribute.role) {
            case peraviz::dmx::AttributeRole::kDimmer:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.dimmer_coarse_offset_1_based,
                                out_offsets.dimmer_fine_offset_1_based,
                                out_offsets.dimmer_ultra_fine_offset_1_based);
                break;
            case peraviz::dmx::AttributeRole::kPan:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.pan_coarse_offset_1_based,
                                out_offsets.pan_fine_offset_1_based,
                                out_offsets.pan_ultra_fine_offset_1_based);
                break;
            case peraviz::dmx::AttributeRole::kTilt:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.tilt_coarse_offset_1_based,
                                out_offsets.tilt_fine_offset_1_based,
                                out_offsets.tilt_ultra_fine_offset_1_based);
                break;
            case peraviz::dmx::AttributeRole::kUnknown:
                break;
            case peraviz::dmx::AttributeRole::kZoom:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.zoom_coarse_offset_1_based,
                                out_offsets.zoom_fine_offset_1_based,
                                out_offsets.zoom_ultra_fine_offset_1_based);
                peraviz::dmx::consume_zoom_physical_range(channel_function, out_offsets);
                break;
            case peraviz::dmx::AttributeRole::kCyan:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.cyan_coarse_offset_1_based,
                                out_offsets.cyan_fine_offset_1_based,
                                out_offsets.cyan_ultra_fine_offset_1_based);
                break;
            case peraviz::dmx::AttributeRole::kMagenta:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.magenta_coarse_offset_1_based,
                                out_offsets.magenta_fine_offset_1_based,
                                out_offsets.magenta_ultra_fine_offset_1_based);
                break;
            case peraviz::dmx::AttributeRole::kYellow:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.yellow_coarse_offset_1_based,
                                out_offsets.yellow_fine_offset_1_based,
                                out_offsets.yellow_ultra_fine_offset_1_based);
                break;
            case peraviz::dmx::AttributeRole::kGobo: {
                const std::string wheel_name =
                    peraviz::dmx::lower_ascii(peraviz::dmx::read_attr_ci(channel_function, "Wheel", "wheel"));
                int wheel_number = parsed_attribute.gobo_wheel_number;
                if (wheel_number <= 0) {
                    wheel_number = parse_last_number_token(wheel_name);
                }
                peraviz::dmx::FixtureGoboWheelOffset *wheel =
                    find_or_create_gobo_wheel_offset(out_offsets, wheel_number, wheel_name);
                if (!wheel) {
                    break;
                }
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                wheel->coarse_offset_1_based,
                                wheel->fine_offset_1_based,
                                wheel->ultra_fine_offset_1_based);
                peraviz::dmx::consume_gobo_channel_sets(channel_function, wheel_catalog, *wheel,
                                                        function_dmx_from, function_dmx_to,
                                                        function_mode_from, function_mode_to);
                break;
            }
            case peraviz::dmx::AttributeRole::kGoboIndex: {
                const std::string wheel_name =
                    peraviz::dmx::lower_ascii(peraviz::dmx::read_attr_ci(channel_function, "Wheel", "wheel"));
                int wheel_number = parsed_attribute.gobo_wheel_number;
                if (wheel_number <= 0) {
                    wheel_number = parse_last_number_token(wheel_name);
                }
                peraviz::dmx::FixtureGoboWheelOffset *wheel =
                    find_or_create_gobo_wheel_offset(out_offsets, wheel_number, wheel_name);
                if (!wheel) {
                    break;
                }
                wheel->supports_index = true;
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                wheel->index_coarse_offset_1_based,
                                wheel->index_fine_offset_1_based,
                                wheel->index_ultra_fine_offset_1_based);
                peraviz::dmx::consume_gobo_physical_range(channel_function,
                                                          wheel->has_index_physical_limits,
                                                          wheel->index_physical_min,
                                                          wheel->index_physical_max);
                break;
            }
            case peraviz::dmx::AttributeRole::kGoboRotation: {
                const std::string wheel_name =
                    peraviz::dmx::lower_ascii(peraviz::dmx::read_attr_ci(channel_function, "Wheel", "wheel"));
                int wheel_number = parsed_attribute.gobo_wheel_number;
                if (wheel_number <= 0) {
                    wheel_number = parse_last_number_token(wheel_name);
                }
                peraviz::dmx::FixtureGoboWheelOffset *wheel =
                    find_or_create_gobo_wheel_offset(out_offsets, wheel_number, wheel_name);
                if (!wheel) {
                    break;
                }

                wheel->supports_rotation = true;
                int candidate_rotation_coarse = -1;
                int candidate_rotation_fine = -1;
                int candidate_rotation_ultra_fine = -1;
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                candidate_rotation_coarse,
                                candidate_rotation_fine,
                                candidate_rotation_ultra_fine);

                const std::string attribute_lower = peraviz::dmx::lower_ascii(peraviz::dmx::trim_ascii(attribute));
                const bool has_mode_master =
                    channel_function->Attribute("ModeMaster") != nullptr ||
                    channel_function->Attribute("modemaster") != nullptr;
                const bool prefers_position_rotation_channel =
                    attribute_lower.find("posrotate") != std::string::npos ||
                    attribute_lower.find("posrotation") != std::string::npos ||
                    (attribute_lower.find("gobo") != std::string::npos &&
                     attribute_lower.find("pos") != std::string::npos &&
                     attribute_lower.find("rotate") != std::string::npos);
                int candidate_priority = 0;
                if (has_mode_master && prefers_position_rotation_channel) {
                    candidate_priority = 3;
                } else if (has_mode_master) {
                    candidate_priority = 2;
                } else if (prefers_position_rotation_channel) {
                    candidate_priority = 1;
                }

                const bool has_valid_candidate_channel = candidate_rotation_coarse > 0;
                const bool should_replace_rotation_channel =
                    has_valid_candidate_channel &&
                    (wheel->rotation_coarse_offset_1_based <= 0 ||
                     candidate_priority > wheel->rotation_channel_priority ||
                     (candidate_priority == wheel->rotation_channel_priority &&
                      candidate_rotation_coarse < wheel->rotation_coarse_offset_1_based));
                if (should_replace_rotation_channel) {
                    wheel->rotation_coarse_offset_1_based = candidate_rotation_coarse;
                    wheel->rotation_fine_offset_1_based = candidate_rotation_fine;
                    wheel->rotation_ultra_fine_offset_1_based = candidate_rotation_ultra_fine;
                    wheel->rotation_channel_priority = candidate_priority;
                }

                peraviz::dmx::consume_gobo_rotation_channel_sets(channel_function,
                                                                 wheel->rotation_ranges,
                                                                 function_dmx_from,
                                                                 function_dmx_to,
                                                                 function_mode_from,
                                                                 function_mode_to);
                break;
            }
            }
        }
    }
}

DimmerResolveCacheEntry resolve_uncached(const std::string &gdtf_path,
                                         const std::string &dmx_mode_name) {
    DimmerResolveCacheEntry out;

    const std::string xml_content = peraviz::dmx::read_gdtf_description_xml(gdtf_path);
    if (xml_content.empty()) {
        out.reason = "GDTF description.xml not found";
        return out;
    }

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml_content.c_str()) != tinyxml2::XML_SUCCESS) {
        out.reason = "GDTF description.xml parse failure";
        return out;
    }

    tinyxml2::XMLElement *root = doc.RootElement();
    if (!root) {
        out.reason = "GDTF XML root missing";
        return out;
    }

    std::vector<tinyxml2::XMLElement *> modes = peraviz::dmx::collect_elements_by_name(root, "dmxmode");
    if (modes.empty()) {
        out.reason = "GDTF DMXMode list is empty";
        return out;
    }

    tinyxml2::XMLElement *selected_mode = nullptr;
    for (tinyxml2::XMLElement *mode : modes) {
        const char *name = mode->Attribute("Name");
        if (!name) {
            name = mode->Attribute("name");
        }
        if (name && dmx_mode_name == name) {
            selected_mode = mode;
            break;
        }
    }

    if (!selected_mode) {
        const std::string expected = peraviz::dmx::lower_ascii(dmx_mode_name);
        for (tinyxml2::XMLElement *mode : modes) {
            const char *name = mode->Attribute("Name");
            if (!name) {
                name = mode->Attribute("name");
            }
            if (name && expected == peraviz::dmx::lower_ascii(name)) {
                selected_mode = mode;
                break;
            }
        }
    }

    if (!selected_mode) {
        out.reason = "DMX mode not found in GDTF: " + dmx_mode_name;
        return out;
    }

    const peraviz::dmx::GoboWheelCatalog wheel_catalog = peraviz::dmx::build_gobo_wheel_catalog(gdtf_path, root);

    std::vector<tinyxml2::XMLElement *> dmx_channels = peraviz::dmx::collect_elements_by_name(selected_mode, "dmxchannel");
    if (dmx_channels.empty()) {
        out.reason = "DMX mode has no DMXChannel entries";
        return out;
    }

    for (tinyxml2::XMLElement *dmx_channel : dmx_channels) {
        consume_channel_offsets(dmx_channel, wheel_catalog, out.offsets);
    }

    for (auto &wheel : out.offsets.gobo_wheels) {
        peraviz::dmx::dedupe_and_sort_gobo_wheel(wheel);
    }
    out.offsets.gobo_wheels.erase(
        std::remove_if(out.offsets.gobo_wheels.begin(), out.offsets.gobo_wheels.end(),
                       [](const peraviz::dmx::FixtureGoboWheelOffset &wheel) {
                           return wheel.coarse_offset_1_based <= 0;
                       }),
        out.offsets.gobo_wheels.end());

    std::sort(out.offsets.gobo_wheels.begin(), out.offsets.gobo_wheels.end(),
              [](const peraviz::dmx::FixtureGoboWheelOffset &a,
                 const peraviz::dmx::FixtureGoboWheelOffset &b) {
                  if (a.wheel_number > 0 && b.wheel_number > 0 && a.wheel_number != b.wheel_number) {
                      return a.wheel_number < b.wheel_number;
                  }
                  if (a.wheel_name != b.wheel_name) {
                      return a.wheel_name < b.wheel_name;
                  }
                  return a.coarse_offset_1_based < b.coarse_offset_1_based;
              });

    const peraviz::dmx::FixtureGoboWheelOffset *primary_wheel = nullptr;
    for (const auto &wheel : out.offsets.gobo_wheels) {
        if (wheel.wheel_number == 1) {
            primary_wheel = &wheel;
            break;
        }
    }
    if (!primary_wheel && !out.offsets.gobo_wheels.empty()) {
        primary_wheel = &out.offsets.gobo_wheels.front();
    }
    if (primary_wheel) {
        out.offsets.gobo_coarse_offset_1_based = primary_wheel->coarse_offset_1_based;
        out.offsets.gobo_fine_offset_1_based = primary_wheel->fine_offset_1_based;
        out.offsets.gobo_ultra_fine_offset_1_based = primary_wheel->ultra_fine_offset_1_based;
        out.offsets.gobo_index_coarse_offset_1_based = primary_wheel->index_coarse_offset_1_based;
        out.offsets.gobo_index_fine_offset_1_based = primary_wheel->index_fine_offset_1_based;
        out.offsets.gobo_index_ultra_fine_offset_1_based = primary_wheel->index_ultra_fine_offset_1_based;
        out.offsets.gobo_rotation_coarse_offset_1_based = primary_wheel->rotation_coarse_offset_1_based;
        out.offsets.gobo_rotation_fine_offset_1_based = primary_wheel->rotation_fine_offset_1_based;
        out.offsets.gobo_rotation_ultra_fine_offset_1_based = primary_wheel->rotation_ultra_fine_offset_1_based;
        out.offsets.gobo_wheel_number = primary_wheel->wheel_number;
        out.offsets.gobo_wheel_name = primary_wheel->wheel_name;
        out.offsets.gobo_slots = primary_wheel->slots;
        out.offsets.gobo_ranges = primary_wheel->ranges;
    }

    if (!out.offsets.has_any()) {
        out.reason = "No Dimmer/Pan/Tilt/Zoom/CMY/Gobo attributes found in mode DMX channels";
        return out;
    }

    out.ok = true;
    return out;
}

} // namespace

namespace peraviz::dmx {

bool resolve_fixture_control_offsets(const std::string &gdtf_path,
                                     const std::string &dmx_mode_name,
                                     FixtureControlOffsets &out_offsets,
                                     std::string &out_debug_reason) {
    const std::string cache_key = make_cache_key(gdtf_path, dmx_mode_name);

    {
        const std::scoped_lock lock(g_cache_mutex);
        auto it = g_cache.find(cache_key);
        if (it != g_cache.end()) {
            out_offsets = it->second.offsets;
            out_debug_reason = it->second.reason;
            return it->second.ok;
        }
    }

    const DimmerResolveCacheEntry resolved = resolve_uncached(gdtf_path, dmx_mode_name);

    {
        const std::scoped_lock lock(g_cache_mutex);
        g_cache[cache_key] = resolved;
    }

    out_offsets = resolved.offsets;
    out_debug_reason = resolved.reason;
    return resolved.ok;
}

} // namespace peraviz::dmx
