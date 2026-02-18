#include "dmx/fixture_dmx_binding.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

struct DimmerResolveCacheEntry {
    bool ok = false;
    peraviz::dmx::FixtureControlOffsets offsets;
    std::string reason;
};

std::mutex g_cache_mutex;
std::unordered_map<std::string, DimmerResolveCacheEntry> g_cache;

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string trim_ascii(std::string text) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [&](unsigned char c) { return !is_space(c); }));
    text.erase(std::find_if(text.rbegin(), text.rend(), [&](unsigned char c) { return !is_space(c); }).base(), text.end());
    return text;
}

std::string make_cache_key(const std::string &gdtf_path, const std::string &dmx_mode_name) {
    return gdtf_path + "\n" + dmx_mode_name;
}

std::string read_gdtf_description_xml(const std::string &gdtf_path) {
    wxFileInputStream input(wxString::FromUTF8(gdtf_path.c_str()));
    if (!input.IsOk()) {
        return {};
    }

    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        const std::string file_name = lower_ascii(entry->GetName().ToUTF8().data());
        if (file_name != "description.xml" && file_name.find("/description.xml") == std::string::npos) {
            continue;
        }

        std::string xml;
        char buffer[4096];
        while (!zip.Eof()) {
            zip.Read(buffer, sizeof(buffer));
            const size_t n = zip.LastRead();
            if (n == 0) {
                break;
            }
            xml.append(buffer, n);
        }
        return xml;
    }

    return {};
}

std::vector<tinyxml2::XMLElement *> collect_elements_by_name(tinyxml2::XMLElement *root, const std::string &name_lower) {
    std::vector<tinyxml2::XMLElement *> result;
    std::vector<tinyxml2::XMLElement *> stack;
    if (root) {
        stack.push_back(root);
    }

    while (!stack.empty()) {
        tinyxml2::XMLElement *node = stack.back();
        stack.pop_back();

        if (lower_ascii(node->Name()) == name_lower) {
            result.push_back(node);
        }

        for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
            stack.push_back(child);
        }
    }
    return result;
}

std::vector<int> parse_offsets(const char *raw_offset) {
    std::vector<int> offsets;
    if (!raw_offset) {
        return offsets;
    }

    std::string offset_text = trim_ascii(raw_offset);
    for (char &ch : offset_text) {
        if (ch == ';') {
            ch = ',';
        }
    }

    std::stringstream ss(offset_text);
    std::string piece;
    while (std::getline(ss, piece, ',')) {
        piece = trim_ascii(piece);
        if (piece.empty()) {
            continue;
        }
        char *end = nullptr;
        const long parsed = std::strtol(piece.c_str(), &end, 10);
        if (end == piece.c_str() || parsed <= 0L) {
            continue;
        }
        offsets.push_back(static_cast<int>(parsed));
    }

    return offsets;
}

enum class AttributeRole {
    kUnknown,
    kDimmer,
    kPan,
    kTilt,
    kZoom,
};

struct ParsedAttribute {
    AttributeRole role = AttributeRole::kUnknown;
    bool is_fine = false;
    int byte_index = 1;
};

bool has_explicit_fine_marker(const std::string &lower) {
    return lower.find("fine") != std::string::npos ||
           lower.find("lsb") != std::string::npos;
}

int parse_compact_byte_index(const std::string &lower, const std::string &role_token) {
    if (lower.rfind(role_token, 0) != 0) {
        return -1;
    }

    std::string rest = trim_ascii(lower.substr(role_token.size()));
    if (rest.empty()) {
        return 1;
    }

    if (rest[0] == '.' || rest[0] == '_' || rest[0] == '-' || rest[0] == ' ') {
        rest.erase(rest.begin());
        rest = trim_ascii(rest);
    }

    if (rest.empty()) {
        return 1;
    }

    size_t digits_len = 0;
    while (digits_len < rest.size() && std::isdigit(static_cast<unsigned char>(rest[digits_len])) != 0) {
        ++digits_len;
    }
    if (digits_len == 0) {
        return -1;
    }

    const std::string suffix = trim_ascii(rest.substr(digits_len));
    if (!suffix.empty()) {
        return -1;
    }

    const long parsed = std::strtol(rest.substr(0, digits_len).c_str(), nullptr, 10);
    if (parsed <= 0L) {
        return -1;
    }
    return static_cast<int>(parsed);
}

std::string last_attribute_segment(const std::string &attribute) {
    const size_t dot = attribute.find_last_of('.');
    if (dot == std::string::npos) {
        return attribute;
    }
    return trim_ascii(attribute.substr(dot + 1));
}

bool starts_with_role_token(const std::string &attribute,
                            const std::string &role_token,
                            int &out_byte_index) {
    if (attribute.rfind(role_token, 0) != 0) {
        return false;
    }

    std::string rest = trim_ascii(attribute.substr(role_token.size()));
    while (!rest.empty() && (rest[0] == '.' || rest[0] == '_' || rest[0] == '-' || rest[0] == ' ')) {
        rest.erase(rest.begin());
        rest = trim_ascii(rest);
    }
    if (rest.empty()) {
        out_byte_index = 1;
        return true;
    }

    if (rest.rfind("fine", 0) == 0 || rest.rfind("lsb", 0) == 0 ||
        rest.rfind("coarse", 0) == 0 || rest.rfind("msb", 0) == 0) {
        out_byte_index = 1;
        return true;
    }

    const int compact_byte_index = parse_compact_byte_index(attribute, role_token);
    if (compact_byte_index > 0) {
        out_byte_index = compact_byte_index;
        return true;
    }

    return false;
}

ParsedAttribute parse_attribute_name(const std::string &raw_attribute) {
    ParsedAttribute parsed;
    const std::string lower = lower_ascii(trim_ascii(raw_attribute));
    if (lower.empty()) {
        return parsed;
    }

    const std::string leaf = last_attribute_segment(lower);

    int byte_index = 1;
    if (starts_with_role_token(leaf, "dimmer", byte_index) ||
        starts_with_role_token(leaf, "intensity", byte_index)) {
        parsed.role = AttributeRole::kDimmer;
        parsed.byte_index = byte_index;
    } else if (starts_with_role_token(leaf, "pan", byte_index)) {
        parsed.role = AttributeRole::kPan;
        parsed.byte_index = byte_index;
    } else if (starts_with_role_token(leaf, "tilt", byte_index)) {
        parsed.role = AttributeRole::kTilt;
        parsed.byte_index = byte_index;
    } else if (starts_with_role_token(leaf, "zoom", byte_index) ||
               starts_with_role_token(leaf, "digitalzoom", byte_index)) {
        parsed.role = AttributeRole::kZoom;
        parsed.byte_index = byte_index;
    }

    if (parsed.role == AttributeRole::kUnknown) {
        return parsed;
    }

    if (has_explicit_fine_marker(lower) || parsed.byte_index >= 2) {
        parsed.is_fine = true;
    }

    return parsed;
}

void consume_zoom_physical_range(tinyxml2::XMLElement *channel_function,
                                 peraviz::dmx::FixtureControlOffsets &out_offsets);

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

void consume_zoom_physical_range(tinyxml2::XMLElement *channel_function,
                                 peraviz::dmx::FixtureControlOffsets &out_offsets) {
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

void consume_channel_offsets(tinyxml2::XMLElement *dmx_channel,
                             peraviz::dmx::FixtureControlOffsets &out_offsets) {
    if (!dmx_channel) {
        return;
    }

    const std::vector<int> offsets = parse_offsets(dmx_channel->Attribute("Offset"));
    if (offsets.empty()) {
        return;
    }

    const std::vector<tinyxml2::XMLElement *> logical_channels = collect_elements_by_name(dmx_channel, "logicalchannel");
    for (tinyxml2::XMLElement *logical_channel : logical_channels) {
        const char *logical_attribute = logical_channel->Attribute("Attribute");
        if (!logical_attribute) {
            logical_attribute = logical_channel->Attribute("attribute");
        }

        const std::vector<tinyxml2::XMLElement *> channel_functions = collect_elements_by_name(logical_channel, "channelfunction");
        for (tinyxml2::XMLElement *channel_function : channel_functions) {
            const char *attribute = channel_function->Attribute("Attribute");
            if (!attribute) {
                attribute = channel_function->Attribute("attribute");
            }
            // According to GDTF, a ChannelFunction may omit Attribute and inherit the
            // parent LogicalChannel attribute. Fall back to LogicalChannel here.
            if (!attribute) {
                attribute = logical_attribute;
            }
            if (!attribute) {
                continue;
            }

            const ParsedAttribute parsed_attribute = parse_attribute_name(attribute);
            switch (parsed_attribute.role) {
            case AttributeRole::kDimmer:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.dimmer_coarse_offset_1_based,
                                out_offsets.dimmer_fine_offset_1_based,
                                out_offsets.dimmer_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kPan:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.pan_coarse_offset_1_based,
                                out_offsets.pan_fine_offset_1_based,
                                out_offsets.pan_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kTilt:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.tilt_coarse_offset_1_based,
                                out_offsets.tilt_fine_offset_1_based,
                                out_offsets.tilt_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kUnknown:
                break;
            case AttributeRole::kZoom:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.zoom_coarse_offset_1_based,
                                out_offsets.zoom_fine_offset_1_based,
                                out_offsets.zoom_ultra_fine_offset_1_based);
                consume_zoom_physical_range(channel_function, out_offsets);
                break;
            }
        }
    }
}

DimmerResolveCacheEntry resolve_uncached(const std::string &gdtf_path,
                                         const std::string &dmx_mode_name) {
    DimmerResolveCacheEntry out;

    const std::string xml_content = read_gdtf_description_xml(gdtf_path);
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

    std::vector<tinyxml2::XMLElement *> modes = collect_elements_by_name(root, "dmxmode");
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
        const std::string expected = lower_ascii(dmx_mode_name);
        for (tinyxml2::XMLElement *mode : modes) {
            const char *name = mode->Attribute("Name");
            if (!name) {
                name = mode->Attribute("name");
            }
            if (name && expected == lower_ascii(name)) {
                selected_mode = mode;
                break;
            }
        }
    }

    if (!selected_mode) {
        out.reason = "DMX mode not found in GDTF: " + dmx_mode_name;
        return out;
    }

    std::vector<tinyxml2::XMLElement *> dmx_channels = collect_elements_by_name(selected_mode, "dmxchannel");
    if (dmx_channels.empty()) {
        out.reason = "DMX mode has no DMXChannel entries";
        return out;
    }

    for (tinyxml2::XMLElement *dmx_channel : dmx_channels) {
        consume_channel_offsets(dmx_channel, out.offsets);
    }

    if (!out.offsets.has_any()) {
        out.reason = "No Dimmer/Pan/Tilt/Zoom attributes found in mode DMX channels";
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
