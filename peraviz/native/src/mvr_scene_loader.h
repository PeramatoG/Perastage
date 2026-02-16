#pragma once

#include <string>

#include "scene_model.h"

namespace peraviz {

SceneModel load_mvr(const std::string &path, bool peraviz_debug_baseline = false,
                    bool peraviz_debug_coords = false);

} // namespace peraviz
