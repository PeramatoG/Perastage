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

  // Always refresh symbol definitions while hidden layers are temporarily
  // disabled, so legends can include fixture types that were not part of the
  // last interactive capture.
  capturePanel->CaptureFrameNow([](CommandBuffer, Viewer2DViewState) {}, true,
                                false);

  auto symbols = capturePanel->GetBottomSymbolCacheSnapshot();
  if (!symbols || symbols->empty() || !requireTopAndFrontViews)
    return symbols;

  bool hasTop = false;
  bool hasFront = false;
  for (const auto &entry : *symbols) {
    if (entry.second.key.viewKind == SymbolViewKind::Top)
      hasTop = true;
    else if (entry.second.key.viewKind == SymbolViewKind::Front)
      hasFront = true;
    if (hasTop && hasFront)
      break;
  }

  if (!hasTop || !hasFront) {
    auto captureMissingView = [&](Viewer2DView view) {
      capturePanel->SetView(view);
      capturePanel->CaptureFrameNow([](CommandBuffer, Viewer2DViewState) {},
                                    true, false);
    };
    if (!hasTop)
      captureMissingView(Viewer2DView::Top);
    if (!hasFront)
      captureMissingView(Viewer2DView::Front);
    capturePanel->SetView(previousView);
    symbols = capturePanel->GetBottomSymbolCacheSnapshot();
  }

  return symbols;
}
} // namespace

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshot(Viewer2DPanel *capturePanel, ConfigManager &cfg,
                            bool requireTopAndFrontViews) {
  ScopedHiddenLayersClear hiddenLayersGuard(cfg);
  return CaptureLegendSymbolSnapshotWithAllLayers(capturePanel,
                                                  requireTopAndFrontViews);
}
