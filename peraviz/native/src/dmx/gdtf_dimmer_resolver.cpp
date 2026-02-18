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
};

struct ParsedAttribute {
    AttributeRole role = AttributeRole::kUnknown;
    bool is_fine = false;
};

bool has_explicit_fine_marker(const std::string &lower) {
    return lower.find("fine") != std::string::npos ||
           lower.find("lsb") != std::string::npos;
}

ParsedAttribute parse_attribute_name(const std::string &raw_attribute) {
    ParsedAttribute parsed;
    const std::string lower = lower_ascii(trim_ascii(raw_attribute));
    if (lower.empty()) {
        return parsed;
    }

    if (lower.rfind("dimmer", 0) == 0 || lower.find("dimmer") != std::string::npos) {
        parsed.role = AttributeRole::kDimmer;
    } else if (lower.rfind("pan", 0) == 0 || lower.find(" pan") != std::string::npos) {
        parsed.role = AttributeRole::kPan;
    } else if (lower.rfind("tilt", 0) == 0 || lower.find(" tilt") != std::string::npos) {
        parsed.role = AttributeRole::kTilt;
    }

    if (parsed.role == AttributeRole::kUnknown) {
        return parsed;
    }

    if (has_explicit_fine_marker(lower)) {
        parsed.is_fine = true;
        return parsed;
    }

    return parsed;
}

void consume_offsets(const std::vector<int> &offsets,
                     bool is_fine,
                     int &coarse,
                     int &fine,
                     int &ultra_fine) {
    if (offsets.empty()) {
        return;
    }

    if (is_fine) {
        const int fine_candidate = offsets[0];
        if (fine <= 0 || fine_candidate < fine) {
            fine = fine_candidate;
        }
        if (offsets.size() > 1 && (ultra_fine <= 0 || offsets[1] < ultra_fine)) {
            ultra_fine = offsets[1];
        }
        if (offsets.size() > 2 && (ultra_fine <= 0 || offsets[2] < ultra_fine)) {
            ultra_fine = offsets[2];
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
        const std::vector<tinyxml2::XMLElement *> channel_functions = collect_elements_by_name(logical_channel, "channelfunction");
        for (tinyxml2::XMLElement *channel_function : channel_functions) {
            const char *attribute = channel_function->Attribute("Attribute");
            if (!attribute) {
                attribute = channel_function->Attribute("attribute");
            }
            if (!attribute) {
                continue;
            }

            const ParsedAttribute parsed_attribute = parse_attribute_name(attribute);
            switch (parsed_attribute.role) {
            case AttributeRole::kDimmer:
                consume_offsets(offsets, parsed_attribute.is_fine,
                                out_offsets.dimmer_coarse_offset_1_based,
                                out_offsets.dimmer_fine_offset_1_based,
                                out_offsets.dimmer_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kPan:
                consume_offsets(offsets, parsed_attribute.is_fine,
                                out_offsets.pan_coarse_offset_1_based,
                                out_offsets.pan_fine_offset_1_based,
                                out_offsets.pan_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kTilt:
                consume_offsets(offsets, parsed_attribute.is_fine,
                                out_offsets.tilt_coarse_offset_1_based,
                                out_offsets.tilt_fine_offset_1_based,
                                out_offsets.tilt_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kUnknown:
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
        out.reason = "No Dimmer/Pan/Tilt attributes found in mode DMX channels";
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
