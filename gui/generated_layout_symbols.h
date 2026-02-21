#pragma once

#include <memory>
#include <string>
#include <vector>

#include "symbolcache.h"

class ConfigManager;
class Viewer2DOffscreenRenderer;

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureGeneratedLayoutSymbolSnapshot(Viewer2DOffscreenRenderer &renderer,
                                     ConfigManager &cfg,
                                     const std::vector<std::string> &modelKeys);
