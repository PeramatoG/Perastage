#pragma once

#include <memory>

#include "symbolcache.h"

class ConfigManager;
class Viewer2DPanel;
enum class Viewer2DView;

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshot(Viewer2DPanel *capturePanel, ConfigManager &cfg,
                            bool requireTopAndFrontViews);
