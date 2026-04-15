#include "dmx/gdtf_attribute_classifier.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string &message) {
    std::cerr << message << std::endl;
    return 1;
}

int expect_parsed(const std::string &attribute,
                  peraviz::dmx::AttributeRole expected_role,
                  bool expected_is_fine,
                  int expected_byte_index,
                  int expected_gobo_wheel_number) {
    const peraviz::dmx::ParsedAttribute parsed = peraviz::dmx::parse_attribute_name(attribute);
    if (parsed.role != expected_role) {
        return fail("Unexpected role for '" + attribute + "'");
    }
    if (parsed.is_fine != expected_is_fine) {
        return fail("Unexpected fine flag for '" + attribute + "'");
    }
    if (parsed.byte_index != expected_byte_index) {
        return fail("Unexpected byte index for '" + attribute + "'");
    }
    if (parsed.gobo_wheel_number != expected_gobo_wheel_number) {
        return fail("Unexpected gobo wheel number for '" + attribute + "'");
    }
    return 0;
}

} // namespace

int main() {
    if (const int rc = expect_parsed("pan", peraviz::dmx::AttributeRole::kPan, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("tilt", peraviz::dmx::AttributeRole::kTilt, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("zoom", peraviz::dmx::AttributeRole::kZoom, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("coloradd_cyan", peraviz::dmx::AttributeRole::kCyan, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("panfine", peraviz::dmx::AttributeRole::kPan, true, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("gobo1posrotate", peraviz::dmx::AttributeRole::kGoboRotation, false, 1, 1); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("gobo2index", peraviz::dmx::AttributeRole::kGoboIndex, false, 1, 2); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("gobo1", peraviz::dmx::AttributeRole::kGobo, false, 1, 1); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("Gobo1SelectShake", peraviz::dmx::AttributeRole::kGobo, false, 1, 1); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("StaticGoboShake", peraviz::dmx::AttributeRole::kGoboRotation, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("Gobo2ShakeIndex", peraviz::dmx::AttributeRole::kGoboRotation, false, 1, 2); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("Gobo2Shaking", peraviz::dmx::AttributeRole::kGoboRotation, false, 1, 2); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("shutterstrobe", peraviz::dmx::AttributeRole::kStrobe, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("prism1pos", peraviz::dmx::AttributeRole::kPrismIndex, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("prism1rotate", peraviz::dmx::AttributeRole::kPrismRotation, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("colorwheel", peraviz::dmx::AttributeRole::kColorWheel, false, 1, 0); rc != 0) {
        return rc;
    }
    if (const int rc = expect_parsed("animationwheelrotate", peraviz::dmx::AttributeRole::kAnimationWheelRotation, false, 1, 0); rc != 0) {
        return rc;
    }

    return 0;
}
