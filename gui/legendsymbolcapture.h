#pragma once

#include <memory>
#include <vector>

#include "symbolcache.h"

class ConfigManager;
class Viewer2DPanel;
enum class Viewer2DView;

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshot(Viewer2DPanel *capturePanel, ConfigManager &cfg,
                            bool requireTopAndFrontViews);

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureSymbolSnapshotForViews(Viewer2DPanel *capturePanel, ConfigManager &cfg,
                              const std::vector<Viewer2DView> &views);
