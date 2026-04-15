#include "dmx/gdtf_attribute_classifier.h"

#include "dmx/gdtf_xml_reader.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace peraviz::dmx {

namespace {

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
           leaf.find("posrot") != std::string::npos ||
           leaf.find("wheelspin") != std::string::npos ||
           leaf.find("wheelrotation") != std::string::npos ||
           leaf.find("rotation") != std::string::npos ||
           leaf.find("spin") != std::string::npos ||
           leaf.find("rotate") != std::string::npos;
}

} // namespace

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

} // namespace peraviz::dmx
