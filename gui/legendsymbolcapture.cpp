#include "legendsymbolcapture.h"

#include <unordered_set>

#include "configmanager.h"
#include "viewer2dpanel.h"

namespace {
class ScopedHiddenLayersClear {
public:
  explicit ScopedHiddenLayersClear(ConfigManager &cfg)
      : cfg_(cfg), previous_(cfg.GetHiddenLayers()) {
    if (!previous_.empty())
      cfg_.SetHiddenLayers({});
  }

  ~ScopedHiddenLayersClear() {
    if (!previous_.empty())
      cfg_.SetHiddenLayers(previous_);
  }

private:
  ConfigManager &cfg_;
  std::unordered_set<std::string> previous_;
};

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshotWithAllLayers(Viewer2DPanel *capturePanel,
                                         bool requireTopAndFrontViews) {
  if (!capturePanel)
    return {};

  const Viewer2DView previousView = capturePanel->GetView();

  auto captureView = [&](Viewer2DView view) {
    capturePanel->SetView(view);
    capturePanel->CaptureFrameNow([](CommandBuffer, Viewer2DViewState) {}, true,
                                  false);
  };

  if (requireTopAndFrontViews) {
    // Capture both required legend views explicitly. A global "has front/top"
    // check is not enough because one model may have Front while others only
    // have Top.
    captureView(Viewer2DView::Top);
    captureView(Viewer2DView::Front);
  } else {
    // Keep a fresh snapshot even when only the current view is needed.
    captureView(previousView);
  }

  capturePanel->SetView(previousView);
  return capturePanel->GetBottomSymbolCacheSnapshot();
}
} // namespace

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshot(Viewer2DPanel *capturePanel, ConfigManager &cfg,
                            bool requireTopAndFrontViews) {
  ScopedHiddenLayersClear hiddenLayersGuard(cfg);
  return CaptureLegendSymbolSnapshotWithAllLayers(capturePanel,
                                                  requireTopAndFrontViews);
}
