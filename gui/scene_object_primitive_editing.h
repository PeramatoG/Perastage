#pragma once

#include <string>

class ConfigManager;
class wxWindow;

namespace scene_object_primitives {

bool EditPrimitiveObjectByUuid(wxWindow *parent, ConfigManager &cfg,
                               const std::string &uuid);

} // namespace scene_object_primitives
