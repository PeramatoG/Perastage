#pragma once

#include <string>
#include <vector>

namespace peraviz::dmx {

enum class AttributeRole {
    kUnknown,
    kDimmer,
    kPan,
    kTilt,
    kZoom,
    kCyan,
    kMagenta,
    kYellow,
    kStrobe,
    kPrism,
    kPrismIndex,
    kPrismRotation,
    kColorWheel,
    kColorWheelRotation,
    kAnimationWheel,
    kAnimationWheelRotation,
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

ParsedAttribute parse_attribute_name(const std::string &raw_attribute);

std::vector<int> parse_offsets(const char *raw_offset);

} // namespace peraviz::dmx
