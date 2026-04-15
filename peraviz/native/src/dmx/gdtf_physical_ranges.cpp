#include "dmx/gdtf_physical_ranges.h"

#include "dmx/gdtf_xml_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace peraviz::dmx {

namespace {

bool parse_float_attr_ci(tinyxml2::XMLElement *node,
                         const char *attr_name,
                         const char *attr_name_alt,
                         float &out_value) {
    if (!node) {
        return false;
    }

    if (node->QueryFloatAttribute(attr_name, &out_value) == tinyxml2::XML_SUCCESS) {
        return true;
    }
    if (node->QueryFloatAttribute(attr_name_alt, &out_value) == tinyxml2::XML_SUCCESS) {
        return true;
    }
    return false;
}

} // namespace

bool infer_rotation_physical_from_name(const std::string &range_name,
                                       float &out_physical_from,
                                       float &out_physical_to) {
    const std::string lower_name = lower_ascii(range_name);
    if (lower_name.empty()) {
        return false;
    }

    const bool is_stop =
        lower_name.find("no rotation") != std::string::npos ||
        lower_name.find("stop") != std::string::npos;
    if (is_stop) {
        out_physical_from = 0.0F;
        out_physical_to = 0.0F;
        return true;
    }

    const bool is_ccw =
        lower_name.find("ccw") != std::string::npos ||
        lower_name.find("counterclockwise") != std::string::npos ||
        lower_name.find("counter clockwise") != std::string::npos ||
        lower_name.find("anticlockwise") != std::string::npos ||
        lower_name.find("anti clockwise") != std::string::npos;
    const bool is_cw =
        lower_name.find("cw") != std::string::npos ||
        lower_name.find("clockwise") != std::string::npos;

    int direction_sign = 0;
    if (is_ccw && !is_cw) {
        direction_sign = -1;
    } else if (is_cw && !is_ccw) {
        direction_sign = 1;
    }
    if (direction_sign == 0) {
        return false;
    }

    constexpr float kDefaultMaxAngularSpeedDegPerSec = 540.0F;
    constexpr float kDefaultMinAngularSpeedDegPerSec = 20.0F;
    const float min_speed = kDefaultMinAngularSpeedDegPerSec * static_cast<float>(direction_sign);
    const float max_speed = kDefaultMaxAngularSpeedDegPerSec * static_cast<float>(direction_sign);

    const size_t fast_pos = lower_name.find("fast");
    const size_t slow_pos = lower_name.find("slow");
    if (fast_pos != std::string::npos && slow_pos != std::string::npos) {
        if (fast_pos < slow_pos) {
            out_physical_from = max_speed;
            out_physical_to = min_speed;
        } else {
            out_physical_from = min_speed;
            out_physical_to = max_speed;
        }
        return true;
    }

    if (fast_pos != std::string::npos) {
        out_physical_from = max_speed;
        out_physical_to = max_speed;
        return true;
    }
    if (slow_pos != std::string::npos) {
        out_physical_from = min_speed;
        out_physical_to = min_speed;
        return true;
    }

    out_physical_from = min_speed;
    out_physical_to = max_speed;
    return true;
}

void consume_zoom_physical_range(tinyxml2::XMLElement *channel_function,
                                 FixtureControlOffsets &out_offsets) {
    float physical_from = 0.0F;
    float physical_to = 0.0F;
    const bool has_physical_from = parse_float_attr_ci(channel_function, "PhysicalFrom", "physicalfrom", physical_from);
    const bool has_physical_to = parse_float_attr_ci(channel_function, "PhysicalTo", "physicalto", physical_to);
    if (!has_physical_from || !has_physical_to) {
        return;
    }

    const float min_value = std::min(physical_from, physical_to);
    const float max_value = std::max(physical_from, physical_to);
    if (max_value <= min_value || max_value <= 0.0F) {
        return;
    }

    if (!out_offsets.has_zoom_physical_limits) {
        out_offsets.zoom_physical_min_degrees = min_value;
        out_offsets.zoom_physical_max_degrees = max_value;
        out_offsets.has_zoom_physical_limits = true;
        return;
    }

    out_offsets.zoom_physical_min_degrees =
        std::min(out_offsets.zoom_physical_min_degrees, min_value);
    out_offsets.zoom_physical_max_degrees =
        std::max(out_offsets.zoom_physical_max_degrees, max_value);
}

void consume_gobo_physical_range(tinyxml2::XMLElement *channel_function,
                                 bool &has_limits,
                                 float &out_min,
                                 float &out_max) {
    float physical_from = 0.0F;
    float physical_to = 0.0F;
    const bool has_physical_from = parse_float_attr_ci(channel_function, "PhysicalFrom", "physicalfrom", physical_from);
    const bool has_physical_to = parse_float_attr_ci(channel_function, "PhysicalTo", "physicalto", physical_to);
    if (!has_physical_from || !has_physical_to) {
        return;
    }

    has_limits = true;
    out_min = physical_from;
    out_max = physical_to;
}

int parse_dmx_value_8bit(const char *raw_value) {
    if (!raw_value) {
        return -1;
    }

    std::string value = trim_ascii(raw_value);
    if (value.empty()) {
        return -1;
    }

    int resolution_bytes = 1;
    size_t slash = value.find('/');
    if (slash != std::string::npos) {
        const std::string value_part = trim_ascii(value.substr(0, slash));
        const std::string resolution_part = trim_ascii(value.substr(slash + 1));
        value = value_part;
        if (!resolution_part.empty()) {
            char *resolution_end = nullptr;
            const long parsed_resolution = std::strtol(resolution_part.c_str(), &resolution_end, 10);
            if (resolution_end != resolution_part.c_str() && parsed_resolution > 0L) {
                resolution_bytes = static_cast<int>(std::clamp(parsed_resolution, 1L, 4L));
            }
        }
    }

    char *end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || parsed < 0L) {
        return -1;
    }

    if (resolution_bytes <= 1) {
        return static_cast<int>(std::clamp(parsed, 0L, 255L));
    }

    if (resolution_bytes == 2) {
        const long clamped_16 = std::clamp(parsed, 0L, 65535L);
        return static_cast<int>(clamped_16 >> 8);
    }

    if (resolution_bytes == 3) {
        const long clamped_24 = std::clamp(parsed, 0L, 16777215L);
        return static_cast<int>(clamped_24 >> 16);
    }

    const long long parsed_64 = std::strtoll(value.c_str(), &end, 10);
    if (end == value.c_str() || parsed_64 < 0LL) {
        return -1;
    }
    const long long clamped_32 = std::clamp(parsed_64, 0LL, 4294967295LL);
    return static_cast<int>(clamped_32 >> 24);
}

void consume_gobo_rotation_channel_sets(tinyxml2::XMLElement *channel_function,
                                        std::vector<FixtureGoboRotationRange> &out_ranges,
                                        int function_dmx_from,
                                        int function_dmx_to,
                                        int mode_window_from,
                                        int mode_window_to) {
    if (!channel_function) {
        return;
    }

    const int clamped_function_from = std::clamp(function_dmx_from, 0, 255);
    const int clamped_function_to = std::clamp(function_dmx_to, clamped_function_from, 255);
    const int clamped_mode_from = std::clamp(mode_window_from, 0, 255);
    const int clamped_mode_to = std::clamp(mode_window_to, clamped_mode_from, 255);
    const auto resolve_channel_set_dmx =
        [clamped_function_from, clamped_function_to](int raw_value) -> int {
            if (raw_value >= clamped_function_from && raw_value <= 255) {
                return std::clamp(raw_value, clamped_function_from, clamped_function_to);
            }
            const int relative_value = clamped_function_from + std::max(0, raw_value);
            return std::clamp(relative_value, clamped_function_from, clamped_function_to);
        };

    float function_physical_from = 0.0F;
    float function_physical_to = 0.0F;
    const bool has_function_physical_from =
        parse_float_attr_ci(channel_function, "PhysicalFrom", "physicalfrom", function_physical_from);
    const bool has_function_physical_to =
        parse_float_attr_ci(channel_function, "PhysicalTo", "physicalto", function_physical_to);
    const bool has_function_physical = has_function_physical_from && has_function_physical_to;

    struct ParsedRange {
        int dmx_start = 0;
        int dmx_end = -1;
        float physical_from = 0.0F;
        float physical_to = 0.0F;
        bool has_physical = false;
        std::string name;
    };
    std::vector<ParsedRange> parsed_ranges;

    for (tinyxml2::XMLElement *channel_set = channel_function->FirstChildElement(); channel_set;
         channel_set = channel_set->NextSiblingElement()) {
        if (lower_ascii(channel_set->Name()) != "channelset") {
            continue;
        }

        int raw_dmx_from = parse_dmx_value_8bit(channel_set->Attribute("DMXFrom"));
        if (raw_dmx_from < 0) {
            raw_dmx_from = parse_dmx_value_8bit(channel_set->Attribute("dmxfrom"));
        }
        if (raw_dmx_from < 0) {
            raw_dmx_from = 0;
        }
        const int dmx_start = resolve_channel_set_dmx(raw_dmx_from);

        int raw_dmx_to = parse_dmx_value_8bit(channel_set->Attribute("DMXTo"));
        if (raw_dmx_to < 0) {
            raw_dmx_to = parse_dmx_value_8bit(channel_set->Attribute("dmxto"));
        }
        const int dmx_end = raw_dmx_to < 0 ? -1 : resolve_channel_set_dmx(raw_dmx_to);

        float physical_from = 0.0F;
        float physical_to = 0.0F;
        const bool has_physical_from = parse_float_attr_ci(channel_set, "PhysicalFrom", "physicalfrom", physical_from);
        const bool has_physical_to = parse_float_attr_ci(channel_set, "PhysicalTo", "physicalto", physical_to);
        const bool has_physical = has_physical_from && has_physical_to;

        const char *name_attr = channel_set->Attribute("Name");
        if (!name_attr) {
            name_attr = channel_set->Attribute("name");
        }
        const std::string set_name = name_attr ? name_attr : "";
        parsed_ranges.push_back({dmx_start, dmx_end, physical_from, physical_to, has_physical, set_name});
    }

    if (parsed_ranges.empty()) {
        if (!has_function_physical) {
            return;
        }

        FixtureGoboRotationRange range;
        range.dmx_from = clamped_function_from;
        range.dmx_to = clamped_function_to;
        range.mode_from_8bit = clamped_mode_from;
        range.mode_to_8bit = clamped_mode_to;
        range.physical_from = function_physical_from;
        range.physical_to = function_physical_to;
        range.is_stop_range = std::fabs(function_physical_from) < 0.0001F &&
                              std::fabs(function_physical_to) < 0.0001F;
        range.is_rotation_channel_range = true;
        out_ranges.push_back(range);
        return;
    }

    std::stable_sort(parsed_ranges.begin(), parsed_ranges.end(),
                     [](const ParsedRange &a, const ParsedRange &b) {
                         return a.dmx_start < b.dmx_start;
                     });

    for (size_t index = 0; index < parsed_ranges.size(); ++index) {
        ParsedRange &row = parsed_ranges[index];
        if (row.dmx_end < 0) {
            if (index + 1 < parsed_ranges.size()) {
                row.dmx_end = std::max(row.dmx_start, parsed_ranges[index + 1].dmx_start - 1);
            } else {
                row.dmx_end = clamped_function_to;
            }
        }

        row.dmx_start = std::clamp(row.dmx_start, clamped_function_from, clamped_function_to);
        row.dmx_end = std::clamp(row.dmx_end, clamped_function_from, clamped_function_to);
        if (row.dmx_end < row.dmx_start) {
            std::swap(row.dmx_start, row.dmx_end);
        }

        const std::string lower_name = lower_ascii(row.name);
        const bool is_named_stop = lower_name.find("no rotation") != std::string::npos ||
                                   lower_name.find("stop") != std::string::npos;
        if (!row.has_physical && has_function_physical) {
            if (is_named_stop) {
                row.physical_from = 0.0F;
                row.physical_to = 0.0F;
            } else {
                const float fn_range =
                    static_cast<float>(clamped_function_to - clamped_function_from);
                if (fn_range > 0.0F) {
                    const float t_from = std::clamp(
                        static_cast<float>(row.dmx_start - clamped_function_from) / fn_range,
                        0.0F, 1.0F);
                    const float t_to = std::clamp(
                        static_cast<float>(row.dmx_end - clamped_function_from) / fn_range,
                        0.0F, 1.0F);
                    row.physical_from = function_physical_from +
                        t_from * (function_physical_to - function_physical_from);
                    row.physical_to = function_physical_from +
                        t_to * (function_physical_to - function_physical_from);
                } else {
                    row.physical_from = function_physical_from;
                    row.physical_to = function_physical_to;
                }
            }
            row.has_physical = true;
        }

        if (!row.has_physical &&
            infer_rotation_physical_from_name(row.name, row.physical_from, row.physical_to)) {
            row.has_physical = true;
        }

        if (!row.has_physical) {
            continue;
        }

        FixtureGoboRotationRange range;
        range.dmx_from = row.dmx_start;
        range.dmx_to = row.dmx_end;
        range.mode_from_8bit = clamped_mode_from;
        range.mode_to_8bit = clamped_mode_to;
        range.physical_from = row.physical_from;
        range.physical_to = row.physical_to;
        range.is_stop_range = std::fabs(row.physical_from) < 0.0001F && std::fabs(row.physical_to) < 0.0001F;
        range.is_rotation_channel_range = true;
        out_ranges.push_back(range);
    }
}

} // namespace peraviz::dmx
