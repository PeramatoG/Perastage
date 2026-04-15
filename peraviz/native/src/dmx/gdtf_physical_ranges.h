#pragma once

#include <vector>

#include "dmx/fixture_dmx_binding.h"

#include <tinyxml2.h>

namespace peraviz::dmx {

int parse_dmx_value_8bit(const char *raw_value);

bool infer_rotation_physical_from_name(const std::string &range_name,
                                       float &out_physical_from,
                                       float &out_physical_to);

void consume_zoom_physical_range(tinyxml2::XMLElement *channel_function,
                                 FixtureControlOffsets &out_offsets);

void consume_gobo_physical_range(tinyxml2::XMLElement *channel_function,
                                 bool &has_limits,
                                 float &out_min,
                                 float &out_max);

void consume_gobo_rotation_channel_sets(tinyxml2::XMLElement *channel_function,
                                        std::vector<FixtureGoboRotationRange> &out_ranges,
                                        int function_dmx_from,
                                        int function_dmx_to,
                                        int mode_window_from,
                                        int mode_window_to);

} // namespace peraviz::dmx
