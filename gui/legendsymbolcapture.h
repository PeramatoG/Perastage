#pragma once

#include <memory>

#include "symbolcache.h"

class ConfigManager;
class Viewer2DOffscreenRenderer;

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshot(Viewer2DOffscreenRenderer *offscreenRenderer,
                            ConfigManager &cfg,
                            bool requireTopAndFrontViews);
