#pragma once

#include "scene_object_primitive_dialogs.h"

class ConfigManager;

namespace scene_object_primitives {

void AddSphereObjects(ConfigManager &cfg, const SphereRequest &request);
void AddCubeObjects(ConfigManager &cfg, const CubeRequest &request);
void AddCylinderObjects(ConfigManager &cfg, const CylinderRequest &request);

} // namespace scene_object_primitives
