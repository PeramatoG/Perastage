#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dmx/fixture_dmx_binding.h"

#include <tinyxml2.h>

namespace peraviz::dmx {

struct GoboWheelDefinition {
    std::vector<int> declared_slot_order;
    std::unordered_set<int> declared_slots;
    std::unordered_map<int, std::string> slot_images;
};

using GoboWheelCatalog = std::unordered_map<std::string, GoboWheelDefinition>;

GoboWheelCatalog build_gobo_wheel_catalog(const std::string &gdtf_path, tinyxml2::XMLElement *root);

void consume_gobo_channel_sets(tinyxml2::XMLElement *channel_function,
                               const GoboWheelCatalog &wheel_catalog,
                               FixtureGoboWheelOffset &out_wheel,
                               int function_dmx_from,
                               int function_dmx_to,
                               int mode_window_from,
                               int mode_window_to);

bool resolve_shake_attribute_amplitude_degrees(tinyxml2::XMLElement *channel_function,
                                               float &out_amplitude_from_degrees,
                                               float &out_amplitude_to_degrees);

void dedupe_and_sort_gobo_wheel(FixtureGoboWheelOffset &wheel);

} // namespace peraviz::dmx
