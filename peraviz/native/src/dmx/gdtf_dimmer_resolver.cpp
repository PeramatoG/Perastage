#include "dmx/fixture_dmx_binding.h"
#include "asset_cache.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <utility>

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

std::vector<tinyxml2::XMLElement *> collect_direct_children_by_name(tinyxml2::XMLElement *root,
                                                                    const std::string &name_lower) {
    std::vector<tinyxml2::XMLElement *> result;
    if (!root) {
        return result;
    }
    for (tinyxml2::XMLElement *child = root->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
        if (lower_ascii(child->Name()) == name_lower) {
            result.push_back(child);
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
    kCyan,
    kMagenta,
    kYellow,
    kGobo,
    kGoboIndex,
    kGoboRotation,
};

struct ParsedAttribute {
    AttributeRole role = AttributeRole::kUnknown;
    bool is_fine = false;
    int byte_index = 1;
    int gobo_wheel_number = 0;
};


int parse_gobo_wheel_number(const std::string &leaf) {
    if (leaf.rfind("gobo", 0) != 0 || leaf.size() <= 4) {
        return 0;
    }
    size_t index = 4;
    size_t digits_end = index;
    while (digits_end < leaf.size() && std::isdigit(static_cast<unsigned char>(leaf[digits_end])) != 0) {
        ++digits_end;
    }
    if (digits_end == index) {
        return 0;
    }
    const long parsed = std::strtol(leaf.substr(index, digits_end - index).c_str(), nullptr, 10);
    if (parsed <= 0L) {
        return 0;
    }
    return static_cast<int>(parsed);
}


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

bool matches_gobo_select_with_embedded_motion(const std::string &leaf) {
    return leaf.find("selectspin") != std::string::npos ||
           leaf.find("selectshake") != std::string::npos ||
           leaf.find("selecteffects") != std::string::npos;
}

bool matches_gobo_attribute(const std::string &leaf) {
    if (matches_gobo_select_with_embedded_motion(leaf)) {
        return true;
    }

    const std::array<std::string, 9> non_projector_tokens = {
        "spin", "shake", "audio", "random", "time", "mspeed", "speed", "reset", "rotate"};
    for (const std::string &token : non_projector_tokens) {
        if (leaf.find(token) != std::string::npos) {
            return false;
        }
    }

    int byte_index = 1;
    if (starts_with_role_token(leaf, "gobo", byte_index)) {
        return true;
    }
    if (starts_with_role_token(leaf, "gobowheel", byte_index) ||
        starts_with_role_token(leaf, "goboindex", byte_index) ||
        starts_with_role_token(leaf, "goboselect", byte_index)) {
        return true;
    }

    const bool references_wheel = leaf.find("wheel") != std::string::npos;
    const bool references_selector =
        leaf.find("slot") != std::string::npos || leaf.find("index") != std::string::npos ||
        leaf.find("select") != std::string::npos || leaf.find("pos") != std::string::npos;
    return references_wheel && references_selector;
}

bool matches_gobo_index_attribute(const std::string &leaf) {
    if (leaf.find("gobo") == std::string::npos) {
        return false;
    }
    if (leaf.find("rotate") != std::string::npos || leaf.find("spin") != std::string::npos) {
        return false;
    }
    return leaf.find("pos") != std::string::npos || leaf.find("index") != std::string::npos;
}

bool matches_gobo_rotation_attribute(const std::string &leaf) {
    if (leaf.find("gobo") == std::string::npos) {
        return false;
    }
    if (matches_gobo_select_with_embedded_motion(leaf)) {
        return false;
    }
    return leaf.find("posrotate") != std::string::npos ||
           leaf.find("wheelspin") != std::string::npos ||
           leaf.find("spin") != std::string::npos ||
           leaf.find("rotate") != std::string::npos;
}

peraviz::dmx::FixtureGoboRangeBehavior parse_gobo_range_behavior(const std::string &channel_set_name) {
    const std::string lower_name = lower_ascii(channel_set_name);
    if (lower_name.find("shake") != std::string::npos) {
        return peraviz::dmx::FixtureGoboRangeBehavior::kShake;
    }
    // Prioritize explicit index/position semantics over generic rotate tokens,
    // because some fixture libraries include both terms in index range names.
    if (lower_name.find("index") != std::string::npos ||
        lower_name.find("pos") != std::string::npos) {
        return peraviz::dmx::FixtureGoboRangeBehavior::kIndex;
    }
    if (lower_name.find("spin") != std::string::npos ||
        lower_name.find("rotate") != std::string::npos) {
        return peraviz::dmx::FixtureGoboRangeBehavior::kRotation;
    }
    return peraviz::dmx::FixtureGoboRangeBehavior::kFixed;
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
    } else if (starts_with_role_token(leaf, "cyan", byte_index) ||
               starts_with_role_token(leaf, "coloradd_c", byte_index) ||
               starts_with_role_token(leaf, "coloradd_cyan", byte_index) ||
               starts_with_role_token(leaf, "coloradd_coarse", byte_index) ||
               starts_with_role_token(leaf, "colorsub_c", byte_index) ||
               starts_with_role_token(leaf, "colorsub_cyan", byte_index) ||
               starts_with_role_token(leaf, "colorsub_coarse", byte_index) ||
               starts_with_role_token(leaf, "colorrgb_c", byte_index) ||
               starts_with_role_token(leaf, "colorrgb_cyan", byte_index)) {
        parsed.role = AttributeRole::kCyan;
        parsed.byte_index = byte_index;
    } else if (starts_with_role_token(leaf, "magenta", byte_index) ||
               starts_with_role_token(leaf, "coloradd_m", byte_index) ||
               starts_with_role_token(leaf, "coloradd_magenta", byte_index) ||
               starts_with_role_token(leaf, "colorsub_m", byte_index) ||
               starts_with_role_token(leaf, "colorsub_magenta", byte_index) ||
               starts_with_role_token(leaf, "colorrgb_m", byte_index) ||
               starts_with_role_token(leaf, "colorrgb_magenta", byte_index)) {
        parsed.role = AttributeRole::kMagenta;
        parsed.byte_index = byte_index;
    } else if (starts_with_role_token(leaf, "yellow", byte_index) ||
               starts_with_role_token(leaf, "coloradd_y", byte_index) ||
               starts_with_role_token(leaf, "coloradd_yellow", byte_index) ||
               starts_with_role_token(leaf, "colorsub_y", byte_index) ||
               starts_with_role_token(leaf, "colorsub_yellow", byte_index) ||
               starts_with_role_token(leaf, "colorrgb_y", byte_index) ||
               starts_with_role_token(leaf, "colorrgb_yellow", byte_index)) {
        parsed.role = AttributeRole::kYellow;
        parsed.byte_index = byte_index;
    } else if (matches_gobo_rotation_attribute(leaf)) {
        parsed.role = AttributeRole::kGoboRotation;
        parsed.byte_index = byte_index;
        parsed.gobo_wheel_number = parse_gobo_wheel_number(leaf);
    } else if (matches_gobo_index_attribute(leaf)) {
        parsed.role = AttributeRole::kGoboIndex;
        parsed.byte_index = byte_index;
        parsed.gobo_wheel_number = parse_gobo_wheel_number(leaf);
    } else if (matches_gobo_attribute(leaf)) {
        parsed.role = AttributeRole::kGobo;
        parsed.byte_index = byte_index;
        parsed.gobo_wheel_number = parse_gobo_wheel_number(leaf);
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

void consume_gobo_physical_range(tinyxml2::XMLElement *channel_function,
                                 bool &has_limits,
                                 float &out_min,
                                 float &out_max);

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

int parse_positive_int(const char *raw) {
    if (!raw) {
        return -1;
    }

    std::string value = trim_ascii(raw);
    if (value.empty()) {
        return -1;
    }

    size_t length = 0;
    while (length < value.size() && std::isdigit(static_cast<unsigned char>(value[length])) != 0) {
        ++length;
    }
    if (length == 0) {
        return -1;
    }

    const long parsed = std::strtol(value.substr(0, length).c_str(), nullptr, 10);
    if (parsed <= 0L) {
        return -1;
    }
    return static_cast<int>(parsed);
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
                resolution_bytes = static_cast<int>(std::clamp(parsed_resolution, 1L, 3L));
            }
        }
    }

    char *end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || parsed < 0L) {
        return -1;
    }

    long max_value = 255L;
    if (resolution_bytes == 2) {
        max_value = 65535L;
    } else if (resolution_bytes == 3) {
        max_value = 16777215L;
    }
    const long clamped = std::clamp(parsed, 0L, max_value);
    if (max_value == 255L) {
        return static_cast<int>(clamped);
    }
    return static_cast<int>((clamped * 255L) / max_value);
}

std::string read_attr_ci(tinyxml2::XMLElement *node, const char *name_a, const char *name_b) {
    if (!node) {
        return {};
    }
    const char *value = node->Attribute(name_a);
    if (!value) {
        value = node->Attribute(name_b);
    }
    return value ? trim_ascii(value) : std::string();
}

std::string find_archive_media_by_stem(const std::string &gdtf_path,
                                       const std::string &media_reference) {
    const std::filesystem::path ref_path = std::filesystem::u8path(trim_ascii(media_reference));
    const std::string wanted_stem = lower_ascii(ref_path.stem().u8string());
    if (wanted_stem.empty()) {
        return {};
    }

    wxFileInputStream input(wxString::FromUTF8(gdtf_path.c_str()));
    if (!input.IsOk()) {
        return {};
    }

    std::string best_match;
    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        std::string entry_name = entry->GetName().ToUTF8().data();
        std::replace(entry_name.begin(), entry_name.end(), '\\', '/');
        const std::string entry_lower = lower_ascii(entry_name);
        const std::filesystem::path entry_path = std::filesystem::u8path(entry_name);
        const std::string entry_ext = lower_ascii(entry_path.extension().u8string());
        if (entry_ext != ".png" && entry_ext != ".jpg" && entry_ext != ".jpeg") {
            continue;
        }
        const std::string entry_stem = lower_ascii(entry_path.stem().u8string());
        if (entry_stem != wanted_stem) {
            continue;
        }

        const bool in_wheels_folder = entry_lower.find("wheels/") != std::string::npos;
        if (in_wheels_folder) {
            return entry_name;
        }
        if (best_match.empty()) {
            best_match = entry_name;
        }
    }

    return best_match;
}

std::string ensure_gobo_media_extracted(const std::string &gdtf_path,
                                        peraviz::ZipAssetCache &cache,
                                        const std::string &media_reference) {
    if (media_reference.empty()) {
        return {};
    }

    const std::string normalized = trim_ascii(media_reference);
    std::vector<std::string> candidates;
    candidates.push_back(normalized);
    candidates.push_back("wheels/" + normalized);
    candidates.push_back("wheels/images/" + normalized);

    const std::string lower = lower_ascii(normalized);
    const bool has_ext = lower.find('.') != std::string::npos;
    if (!has_ext) {
        candidates.push_back("wheels/" + normalized + ".png");
        candidates.push_back("wheels/" + normalized + ".jpg");
        candidates.push_back("wheels/" + normalized + ".jpeg");
        candidates.push_back("wheels/images/" + normalized + ".png");
        candidates.push_back("wheels/images/" + normalized + ".jpg");
        candidates.push_back("wheels/images/" + normalized + ".jpeg");
    }

    for (const std::string &candidate : candidates) {
        const std::string extracted = cache.ensure_extracted(candidate);
        if (!extracted.empty()) {
            return extracted;
        }
    }

    const std::string archive_match = find_archive_media_by_stem(gdtf_path, normalized);
    if (!archive_match.empty()) {
        return cache.ensure_extracted(archive_match);
    }

    return {};
}

struct GoboWheelDefinition {
    std::vector<int> declared_slot_order;
    std::unordered_set<int> declared_slots;
    std::unordered_map<int, std::string> slot_images;
};

typedef std::unordered_map<std::string, GoboWheelDefinition> GoboWheelCatalog;

GoboWheelCatalog build_gobo_wheel_catalog(const std::string &gdtf_path, tinyxml2::XMLElement *root) {
    GoboWheelCatalog out;
    if (!root) {
        return out;
    }

    peraviz::ZipAssetCache cache(gdtf_path);
    const std::vector<tinyxml2::XMLElement *> wheels = collect_elements_by_name(root, "wheel");
    for (tinyxml2::XMLElement *wheel : wheels) {
        const std::string wheel_name = lower_ascii(read_attr_ci(wheel, "Name", "name"));
        if (wheel_name.empty()) {
            continue;
        }

        int implicit_index = 1;
        for (tinyxml2::XMLElement *slot_node = wheel->FirstChildElement(); slot_node;
             slot_node = slot_node->NextSiblingElement()) {
            const std::string slot_tag = lower_ascii(slot_node->Name());
            // GDTF 1.0/1.1/1.2 commonly uses <Slot>, while some exporters emit <WheelSlot>.
            if (slot_tag != "slot" && slot_tag != "wheelslot") {
                continue;
            }

            int slot_index = parse_positive_int(slot_node->Attribute("WheelSlotIndex"));
            if (slot_index <= 0) {
                slot_index = parse_positive_int(slot_node->Attribute("wheelslotindex"));
            }
            if (slot_index <= 0) {
                slot_index = implicit_index;
            }
            implicit_index = std::max(implicit_index, slot_index + 1);

            GoboWheelDefinition &wheel_definition = out[wheel_name];
            if (wheel_definition.declared_slots.insert(slot_index).second) {
                wheel_definition.declared_slot_order.push_back(slot_index);
            }

            const std::string media_file = read_attr_ci(slot_node, "MediaFileName", "mediafilename");
            if (media_file.empty()) {
                continue;
            }

            const std::string extracted = ensure_gobo_media_extracted(gdtf_path, cache, media_file);
            if (!extracted.empty()) {
                wheel_definition.slot_images[slot_index] = extracted;
            }
        }
    }

    return out;
}


int normalize_gobo_slot_index(int slot_index,
                              const std::unordered_set<int> &known_slots,
                              const std::vector<int> &known_slot_order) {
    if (slot_index <= 0) {
        return -1;
    }
    if (known_slots.find(slot_index) != known_slots.end()) {
        return slot_index;
    }

    if (known_slot_order.empty()) {
        return -1;
    }

    // Compatibility fallback for fixtures that encode repeated gobo cycles with
    // out-of-range WheelSlotIndex values (e.g. 1..N then N+1..2N).
    // Use fixture-declared slot order instead of assuming contiguous 1..N indices.
    const int wrapped_index = (slot_index - 1) % static_cast<int>(known_slot_order.size());
    const int wrapped_slot = known_slot_order[wrapped_index];
    if (known_slots.find(wrapped_slot) != known_slots.end()) {
        return wrapped_slot;
    }
    return -1;
}

void consume_gobo_channel_sets(tinyxml2::XMLElement *channel_function,
                               const GoboWheelCatalog &wheel_catalog,
                               peraviz::dmx::FixtureGoboWheelOffset &out_wheel,
                               int function_dmx_from,
                               int function_dmx_to) {
    if (!channel_function) {
        return;
    }

    const std::string wheel_name = lower_ascii(read_attr_ci(channel_function, "Wheel", "wheel"));
    if (wheel_name.empty()) {
        return;
    }

    auto wheel_it = wheel_catalog.find(wheel_name);
    if (wheel_it == wheel_catalog.end()) {
        return;
    }

    out_wheel.wheel_name = wheel_name;
    const std::string function_name = read_attr_ci(channel_function, "Name", "name");
    const peraviz::dmx::FixtureGoboRangeBehavior function_behavior_hint =
        parse_gobo_range_behavior(function_name);

    const std::vector<int> &known_slot_order = wheel_it->second.declared_slot_order;
    std::unordered_set<int> known_slots = wheel_it->second.declared_slots;
    for (const auto &[slot_index, image_path] : wheel_it->second.slot_images) {
        if (slot_index <= 0 || image_path.empty()) {
            continue;
        }
        out_wheel.slots.push_back({slot_index, image_path});
    }

    struct ParsedGoboSet {
        int dmx_from = 0;
        int dmx_to = -1;
        int slot_index = -1;
        peraviz::dmx::FixtureGoboRangeBehavior behavior =
            peraviz::dmx::FixtureGoboRangeBehavior::kFixed;
    };
    std::vector<ParsedGoboSet> parsed_sets;

    const int clamped_function_from = std::clamp(function_dmx_from, 0, 255);
    const int clamped_function_to = std::clamp(function_dmx_to, clamped_function_from, 255);

    const auto resolve_channel_set_dmx =
        [clamped_function_from, clamped_function_to](int raw_value) -> int {
        // GDTF libraries may encode ChannelSet DMX values either as absolute
        // channel values or as values relative to the parent ChannelFunction.
        if (raw_value >= clamped_function_from && raw_value <= 255) {
            return std::clamp(raw_value, clamped_function_from, clamped_function_to);
        }
        const int relative_value = clamped_function_from + std::max(0, raw_value);
        return std::clamp(relative_value, clamped_function_from, clamped_function_to);
    };

    int last_slot_index = -1;
    for (tinyxml2::XMLElement *channel_set = channel_function->FirstChildElement(); channel_set;
         channel_set = channel_set->NextSiblingElement()) {
        if (lower_ascii(channel_set->Name()) != "channelset") {
            continue;
        }

        int slot_index = parse_positive_int(channel_set->Attribute("WheelSlotIndex"));
        if (slot_index <= 0) {
            slot_index = parse_positive_int(channel_set->Attribute("wheelslotindex"));
        }
        // GDTF ChannelSet may omit WheelSlotIndex in motion ranges (index/spin/shake).
        // Reuse the previous valid slot so behavior is tied to the selected gobo slot.
        if (slot_index <= 0) {
            slot_index = last_slot_index;
        }
        slot_index = normalize_gobo_slot_index(slot_index, known_slots, known_slot_order);
        if (slot_index <= 0) {
            continue;
        }
        last_slot_index = slot_index;

        int raw_dmx_from = parse_dmx_value_8bit(channel_set->Attribute("DMXFrom"));
        if (raw_dmx_from < 0) {
            raw_dmx_from = parse_dmx_value_8bit(channel_set->Attribute("dmxfrom"));
        }
        if (raw_dmx_from < 0) {
            raw_dmx_from = 0;
        }
        const int dmx_from = resolve_channel_set_dmx(raw_dmx_from);

        int raw_dmx_to = parse_dmx_value_8bit(channel_set->Attribute("DMXTo"));
        if (raw_dmx_to < 0) {
            raw_dmx_to = parse_dmx_value_8bit(channel_set->Attribute("dmxto"));
        }
        const int dmx_to = raw_dmx_to < 0 ? -1 : resolve_channel_set_dmx(raw_dmx_to);

        const std::string channel_set_name = read_attr_ci(channel_set, "Name", "name");
        peraviz::dmx::FixtureGoboRangeBehavior behavior =
            parse_gobo_range_behavior(channel_set_name);
        if (behavior == peraviz::dmx::FixtureGoboRangeBehavior::kFixed &&
            function_behavior_hint != peraviz::dmx::FixtureGoboRangeBehavior::kFixed) {
            behavior = function_behavior_hint;
        }
        parsed_sets.push_back({dmx_from, dmx_to, slot_index, behavior});
    }

    if (parsed_sets.empty()) {
        return;
    }

    std::stable_sort(parsed_sets.begin(), parsed_sets.end(),
                     [](const ParsedGoboSet &a, const ParsedGoboSet &b) {
                         return a.dmx_from < b.dmx_from;
                     });

    for (size_t i = 0; i < parsed_sets.size(); ++i) {
        ParsedGoboSet &row = parsed_sets[i];
        if (row.dmx_to < 0) {
            if (i + 1 < parsed_sets.size()) {
                row.dmx_to = std::max(row.dmx_from, parsed_sets[i + 1].dmx_from - 1);
            } else {
                row.dmx_to = clamped_function_to;
            }
        }
        row.dmx_from = std::clamp(row.dmx_from, clamped_function_from, clamped_function_to);
        row.dmx_to = std::clamp(row.dmx_to, clamped_function_from, clamped_function_to);
        if (row.dmx_to < row.dmx_from) {
            std::swap(row.dmx_from, row.dmx_to);
        }

        out_wheel.ranges.push_back({row.dmx_from, row.dmx_to, row.slot_index, row.behavior});
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

void dedupe_and_sort_gobo_wheel(peraviz::dmx::FixtureGoboWheelOffset &wheel) {
    std::sort(wheel.slots.begin(), wheel.slots.end(),
              [](const peraviz::dmx::FixtureGoboSlot &a,
                 const peraviz::dmx::FixtureGoboSlot &b) {
                  if (a.slot_index != b.slot_index) {
                      return a.slot_index < b.slot_index;
                  }
                  return a.image_path < b.image_path;
              });
    wheel.slots.erase(
        std::unique(wheel.slots.begin(), wheel.slots.end(),
                    [](const peraviz::dmx::FixtureGoboSlot &a,
                       const peraviz::dmx::FixtureGoboSlot &b) {
                        return a.slot_index == b.slot_index && a.image_path == b.image_path;
                    }),
        wheel.slots.end());

    // Keep fixture-authored order for ranges sharing the same DMXFrom, because
    // some libraries use repeated slot entries at identical boundaries to
    // encode different behaviors (index/spin/shake) for the same gobo slot.
    std::stable_sort(wheel.ranges.begin(), wheel.ranges.end(),
                     [](const peraviz::dmx::FixtureGoboRange &a,
                        const peraviz::dmx::FixtureGoboRange &b) {
                         if (a.dmx_from != b.dmx_from) {
                             return a.dmx_from < b.dmx_from;
                         }
                         return a.dmx_to < b.dmx_to;
                     });
    wheel.ranges.erase(
        std::unique(wheel.ranges.begin(), wheel.ranges.end(),
                    [](const peraviz::dmx::FixtureGoboRange &a,
                       const peraviz::dmx::FixtureGoboRange &b) {
                        return a.dmx_from == b.dmx_from && a.dmx_to == b.dmx_to &&
                               a.slot_index == b.slot_index && a.behavior == b.behavior;
                    }),
        wheel.ranges.end());
}

void consume_channel_offsets(tinyxml2::XMLElement *dmx_channel,
                             const GoboWheelCatalog &wheel_catalog,
                             peraviz::dmx::FixtureControlOffsets &out_offsets) {
    if (!dmx_channel) {
        return;
    }

    const std::vector<int> offsets = parse_offsets(dmx_channel->Attribute("Offset"));
    if (offsets.empty()) {
        return;
    }

    const std::vector<tinyxml2::XMLElement *> logical_channels = collect_direct_children_by_name(dmx_channel, "logicalchannel");
    for (tinyxml2::XMLElement *logical_channel : logical_channels) {
        const char *logical_attribute = logical_channel->Attribute("Attribute");
        if (!logical_attribute) {
            logical_attribute = logical_channel->Attribute("attribute");
        }

        const std::vector<tinyxml2::XMLElement *> channel_functions = collect_direct_children_by_name(logical_channel, "channelfunction");
        std::vector<int> function_dmx_from_values;
        function_dmx_from_values.reserve(channel_functions.size());
        for (tinyxml2::XMLElement *channel_function : channel_functions) {
            int function_dmx_from = parse_dmx_value_8bit(channel_function->Attribute("DMXFrom"));
            if (function_dmx_from < 0) {
                function_dmx_from = parse_dmx_value_8bit(channel_function->Attribute("dmxfrom"));
            }
            if (function_dmx_from < 0) {
                function_dmx_from = 0;
            }
            function_dmx_from_values.push_back(std::clamp(function_dmx_from, 0, 255));
        }

        for (size_t channel_function_index = 0;
             channel_function_index < channel_functions.size();
             ++channel_function_index) {
            tinyxml2::XMLElement *channel_function = channel_functions[channel_function_index];
            const int function_dmx_from = function_dmx_from_values[channel_function_index];
            int function_dmx_to = 255;
            if (channel_function_index + 1 < channel_functions.size()) {
                function_dmx_to = function_dmx_from_values[channel_function_index + 1] - 1;
            }
            function_dmx_to = std::clamp(function_dmx_to, function_dmx_from, 255);
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
            case AttributeRole::kCyan:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.cyan_coarse_offset_1_based,
                                out_offsets.cyan_fine_offset_1_based,
                                out_offsets.cyan_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kMagenta:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.magenta_coarse_offset_1_based,
                                out_offsets.magenta_fine_offset_1_based,
                                out_offsets.magenta_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kYellow:
                consume_offsets(offsets, parsed_attribute.is_fine, parsed_attribute.byte_index,
                                out_offsets.yellow_coarse_offset_1_based,
                                out_offsets.yellow_fine_offset_1_based,
                                out_offsets.yellow_ultra_fine_offset_1_based);
                break;
            case AttributeRole::kGobo: {
                const std::string wheel_name = lower_ascii(read_attr_ci(channel_function, "Wheel", "wheel"));
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
                consume_gobo_channel_sets(channel_function, wheel_catalog, *wheel,
                                          function_dmx_from, function_dmx_to);
                break;
            }
            case AttributeRole::kGoboIndex: {
                const std::string wheel_name = lower_ascii(read_attr_ci(channel_function, "Wheel", "wheel"));
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
                                wheel->index_coarse_offset_1_based,
                                wheel->index_fine_offset_1_based,
                                wheel->index_ultra_fine_offset_1_based);
                consume_gobo_physical_range(channel_function,
                                            wheel->has_index_physical_limits,
                                            wheel->index_physical_min,
                                            wheel->index_physical_max);
                break;
            }
            case AttributeRole::kGoboRotation: {
                const std::string wheel_name = lower_ascii(read_attr_ci(channel_function, "Wheel", "wheel"));
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
                                wheel->rotation_coarse_offset_1_based,
                                wheel->rotation_fine_offset_1_based,
                                wheel->rotation_ultra_fine_offset_1_based);
                consume_gobo_physical_range(channel_function,
                                            wheel->has_rotation_physical_limits,
                                            wheel->rotation_physical_min,
                                            wheel->rotation_physical_max);
                break;
            }
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

    const GoboWheelCatalog wheel_catalog = build_gobo_wheel_catalog(gdtf_path, root);

    std::vector<tinyxml2::XMLElement *> dmx_channels = collect_elements_by_name(selected_mode, "dmxchannel");
    if (dmx_channels.empty()) {
        out.reason = "DMX mode has no DMXChannel entries";
        return out;
    }

    for (tinyxml2::XMLElement *dmx_channel : dmx_channels) {
        consume_channel_offsets(dmx_channel, wheel_catalog, out.offsets);
    }

    for (auto &wheel : out.offsets.gobo_wheels) {
        dedupe_and_sort_gobo_wheel(wheel);
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
