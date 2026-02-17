#include "dmx/fixture_dmx_binding.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <unordered_map>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

struct DimmerResolveCacheEntry {
    bool ok = false;
    int offset = -1;
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

int parse_offset_first_component(const char *raw_offset) {
    if (!raw_offset) {
        return -1;
    }

    std::string offset_text = raw_offset;
    offset_text = trim_ascii(offset_text);
    for (char &ch : offset_text) {
        if (ch == ';') {
            ch = ',';
        }
    }

    std::stringstream ss(offset_text);
    std::string first;
    if (!std::getline(ss, first, ',')) {
        return -1;
    }
    first = trim_ascii(first);
    if (first.empty()) {
        return -1;
    }

    char *end = nullptr;
    const long parsed = std::strtol(first.c_str(), &end, 10);
    if (end == first.c_str() || parsed <= 0L) {
        return -1;
    }

    return static_cast<int>(parsed);
}

bool channel_has_dimmer_attribute(tinyxml2::XMLElement *dmx_channel) {
    if (!dmx_channel) {
        return false;
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
            if (lower_ascii(attribute) == "dimmer") {
                return true;
            }
        }
    }

    return false;
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

    int found_count = 0;
    for (tinyxml2::XMLElement *dmx_channel : dmx_channels) {
        if (!channel_has_dimmer_attribute(dmx_channel)) {
            continue;
        }

        const int offset = parse_offset_first_component(dmx_channel->Attribute("Offset"));
        if (offset <= 0) {
            continue;
        }

        ++found_count;
        if (!out.ok) {
            out.ok = true;
            out.offset = offset;
            out.reason.clear();
        }
    }

    if (!out.ok) {
        out.reason = "Dimmer attribute not found in mode DMX channels";
        return out;
    }

    if (found_count > 1) {
        out.reason = "multiple dimmer channels found; using first";
    }

    return out;
}

} // namespace

namespace peraviz::dmx {

bool resolve_dimmer_channel_offset(const std::string &gdtf_path,
                                   const std::string &dmx_mode_name,
                                   int &out_offset_1_based,
                                   std::string &out_debug_reason) {
    const std::string cache_key = make_cache_key(gdtf_path, dmx_mode_name);

    {
        const std::scoped_lock lock(g_cache_mutex);
        auto it = g_cache.find(cache_key);
        if (it != g_cache.end()) {
            out_offset_1_based = it->second.offset;
            out_debug_reason = it->second.reason;
            return it->second.ok;
        }
    }

    const DimmerResolveCacheEntry resolved = resolve_uncached(gdtf_path, dmx_mode_name);

    {
        const std::scoped_lock lock(g_cache_mutex);
        g_cache[cache_key] = resolved;
    }

    out_offset_1_based = resolved.offset;
    out_debug_reason = resolved.reason;
    return resolved.ok;
}

} // namespace peraviz::dmx
