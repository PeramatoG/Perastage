#include "dmx/gdtf_attribute_classifier.h"

#include "dmx/gdtf_xml_reader.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <string_view>
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
           leaf.find("shake") != std::string::npos ||
           leaf.find("shaking") != std::string::npos ||
           leaf.find("spin") != std::string::npos ||
           leaf.find("rotate") != std::string::npos;
}

struct AttributeNameDescriptor {
    AttributeRole role;
    std::initializer_list<std::string_view> tokens;
};

bool match_descriptor_tokens(const std::string &leaf,
                             const AttributeNameDescriptor &descriptor,
                             int &out_byte_index) {
    for (const std::string_view token : descriptor.tokens) {
        int candidate_byte_index = 1;
        if (starts_with_role_token(leaf, std::string(token), candidate_byte_index)) {
            out_byte_index = candidate_byte_index;
            return true;
        }
    }
    return false;
}

const std::array<AttributeNameDescriptor, 7> &attribute_name_descriptors() {
    static const std::array<AttributeNameDescriptor, 7> descriptors = {{
        {AttributeRole::kDimmer, {"dimmer", "intensity"}},
        {AttributeRole::kPan, {"pan"}},
        {AttributeRole::kTilt, {"tilt"}},
        {AttributeRole::kZoom, {"zoom", "digitalzoom"}},
        {AttributeRole::kCyan, {"cyan",
                                "coloradd_c",
                                "coloradd_cyan",
                                "coloradd_coarse",
                                "colorsub_c",
                                "colorsub_cyan",
                                "colorsub_coarse",
                                "colorrgb_c",
                                "colorrgb_cyan"}},
        {AttributeRole::kMagenta, {"magenta",
                                   "coloradd_m",
                                   "coloradd_magenta",
                                   "colorsub_m",
                                   "colorsub_magenta",
                                   "colorrgb_m",
                                   "colorrgb_magenta"}},
        {AttributeRole::kYellow, {"yellow",
                                  "coloradd_y",
                                  "coloradd_yellow",
                                  "colorsub_y",
                                  "colorsub_yellow",
                                  "colorrgb_y",
                                  "colorrgb_yellow"}},
    }};
    return descriptors;
}

bool parse_standard_attribute(const std::string &leaf, ParsedAttribute &parsed) {
    for (const AttributeNameDescriptor &descriptor : attribute_name_descriptors()) {
        int byte_index = 1;
        if (!match_descriptor_tokens(leaf, descriptor, byte_index)) {
            continue;
        }
        parsed.role = descriptor.role;
        parsed.byte_index = byte_index;
        return true;
    }
    return false;
}

bool token_contains_any(const std::string &leaf,
                        const std::initializer_list<std::string_view> &tokens) {
    for (const std::string_view token : tokens) {
        if (leaf.find(std::string(token)) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void parse_wheel_and_prism_attributes(const std::string &leaf, ParsedAttribute &parsed) {
    const bool references_prism = token_contains_any(leaf, {"prism"});
    if (references_prism) {
        if (token_contains_any(leaf, {"rotation", "rotate", "spin"})) {
            parsed.role = AttributeRole::kPrismRotation;
            parsed.byte_index = 1;
            return;
        }
        if (token_contains_any(leaf, {"index", "pos"})) {
            parsed.role = AttributeRole::kPrismIndex;
            parsed.byte_index = 1;
            return;
        }
        parsed.role = AttributeRole::kPrism;
        parsed.byte_index = 1;
        return;
    }

    const bool references_animation_wheel =
        token_contains_any(leaf, {"animationwheel", "animwheel", "animationdisc", "animdisc"});
    if (references_animation_wheel) {
        if (token_contains_any(leaf, {"rotation", "rotate", "spin"})) {
            parsed.role = AttributeRole::kAnimationWheelRotation;
            parsed.byte_index = 1;
            return;
        }
        parsed.role = AttributeRole::kAnimationWheel;
        parsed.byte_index = 1;
        return;
    }

    const bool references_color_wheel =
        token_contains_any(leaf, {"colorwheel", "colourwheel", "colwheel", "colorselect"});
    if (references_color_wheel) {
        if (token_contains_any(leaf, {"rotation", "rotate", "spin"})) {
            parsed.role = AttributeRole::kColorWheelRotation;
            parsed.byte_index = 1;
            return;
        }
        parsed.role = AttributeRole::kColorWheel;
        parsed.byte_index = 1;
        return;
    }

    const bool references_strobe = token_contains_any(leaf, {"strobe", "shutter", "strobe_shutter"});
    if (references_strobe) {
        parsed.role = AttributeRole::kStrobe;
        parsed.byte_index = 1;
    }
}

void parse_gobo_attribute(const std::string &leaf, ParsedAttribute &parsed) {
    if (matches_gobo_rotation_attribute(leaf)) {
        parsed.role = AttributeRole::kGoboRotation;
    } else if (matches_gobo_index_attribute(leaf)) {
        parsed.role = AttributeRole::kGoboIndex;
    } else if (matches_gobo_attribute(leaf)) {
        parsed.role = AttributeRole::kGobo;
    } else {
        return;
    }
    parsed.byte_index = 1;
    parsed.gobo_wheel_number = parse_gobo_wheel_number(leaf);
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

    if (!parse_standard_attribute(leaf, parsed)) {
        parse_wheel_and_prism_attributes(leaf, parsed);
    }

    if (parsed.role == AttributeRole::kUnknown) {
        parse_gobo_attribute(leaf, parsed);
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
