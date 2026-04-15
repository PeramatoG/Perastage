#pragma once

#include <string>

#include "dmx/fixture_dmx_binding.h"

namespace peraviz::dmx {

bool resolve_fixture_control_offsets(const std::string &gdtf_path,
                                     const std::string &dmx_mode_name,
                                     FixtureControlOffsets &out_offsets,
                                     std::string &out_debug_reason);

} // namespace peraviz::dmx
