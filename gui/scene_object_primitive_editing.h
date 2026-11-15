#pragma once

#include <string>

class ConfigManager;
class wxWindow;
struct SceneObject;

namespace scene_object_primitives {

bool CanEditPrimitiveObject(const SceneObject &object);

bool EditPrimitiveObjectByUuid(wxWindow *parent, ConfigManager &cfg,
                               const std::string &uuid);

} // namespace scene_object_primitives
