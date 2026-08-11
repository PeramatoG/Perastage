#include "tools/scene_model_symbol_capture_service.h"

#include <array>
#include <chrono>
#include <string>

#include "configmanager.h"
#include "fixture.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "sceneobject.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "symbols/SymbolGeometrySimplifier.h"
#include "tools/fixture_geometry_bounds.h"
#include "tools/scoped_scene_replacement_lifecycle.h"
#include "tools/scoped_single_model_capture_scene.h"
#include "truss.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"

namespace tools {
namespace {

constexpr float kSymbolRdpEpsilon = 1.0f;

// Temporarily overrides the isolated fixture color used for extraction.
class ScopedFixtureCaptureColor {
public:
  // Applies the requested fixture color when the isolated fixture exists.
  ScopedFixtureCaptureColor(ConfigManager &cfg, const std::string &fixtureUuid,
                            const std::optional<std::string> &forcedHex)
      : cfg_(cfg), fixtureUuid_(fixtureUuid) {
    if (!forcedHex || fixtureUuid_.empty())
      return;
    const auto it = cfg_.GetScene().fixtures.find(fixtureUuid_);
    if (it == cfg_.GetScene().fixtures.end())
      return;
    previous_ = it->second.visualColorHex;
    it->second.visualColorHex = *forcedHex;
  }

  // Restores the isolated fixture color before the project scene returns.
  ~ScopedFixtureCaptureColor() {
    if (!previous_)
      return;
    const auto it = cfg_.GetScene().fixtures.find(fixtureUuid_);
    if (it != cfg_.GetScene().fixtures.end())
      it->second.visualColorHex = *previous_;
  }

private:
  ConfigManager &cfg_;
  std::string fixtureUuid_;
  std::optional<std::string> previous_;
};

// Saves and restores 2D viewer state so capture does not affect interactive UI
// state.
class ScopedViewer2DCaptureState {
public:
  // Stores the current 2D viewer state so capture can run without persisting UI
  // changes.
  explicit ScopedViewer2DCaptureState(Viewer2DPanel &panel)
      : panel_(panel),
        lifecycle_([&panel]() { panel.PrepareForSceneReplacement(); },
                   [&panel]() { panel.CompleteSceneReplacement(); },
                   [&panel]() { panel.UpdateScene(false); }) {
    const Viewer2DViewState state = panel_.GetViewState();
    offsetXPixels_ = state.offsetPixelsX;
    offsetYPixels_ = state.offsetPixelsY;
    zoom_ = state.zoom;
    view_ = state.view;
    renderMode_ = panel_.GetRenderMode();
    renderOverrides_ = panel_.GetRenderOverrides();
    preferPerastageSvgSymbolsForLayouts_ =
        panel_.GetPreferPerastageSvgSymbolsForLayouts();
  }

  // Restores the saved 2D viewer state after capture operations finish.
  ~ScopedViewer2DCaptureState() {
    panel_.ApplyViewState(offsetXPixels_, offsetYPixels_, zoom_, view_,
                          renderMode_);
    panel_.SetRenderOverrides(renderOverrides_);
    panel_.SetPreferPerastageSvgSymbolsForLayouts(
        preferPerastageSvgSymbolsForLayouts_);
  }

  // Completes replacement after the isolated snapshot becomes active.
  void CompleteReplacement() { lifecycle_.CompleteReplacement(); }

  // Prepares replacement automatically before the isolated snapshot is
  // released.
  ScopedPrepareSceneReplacement PrepareOnScopeExit() {
    return lifecycle_.PrepareOnScopeExit();
  }

private:
  Viewer2DPanel &panel_;
  float offsetXPixels_ = 0.0f;
  float offsetYPixels_ = 0.0f;
  float zoom_ = 1.0f;
  Viewer2DView view_ = Viewer2DView::Top;
  Viewer2DRenderMode renderMode_ = Viewer2DRenderMode::White;
  std::optional<Viewer2DRenderOverrides> renderOverrides_;
  bool preferPerastageSvgSymbolsForLayouts_ = false;
  ScopedSceneReplacementLifecycle lifecycle_;
};

// Applies temporary render overrides tailored for high-contrast symbol
// extraction.
class ScopedViewer2DRenderOverrides {
public:
  // Applies temporary render overrides optimized for symbol capture.
  ScopedViewer2DRenderOverrides(Viewer2DPanel &panel,
                                const Viewer2DRenderOverrides &overrides)
      : panel_(panel) {
    panel_.SetRenderOverrides(overrides);
  }

  // Clears temporary render overrides when capture scope ends.
  ~ScopedViewer2DRenderOverrides() { panel_.SetRenderOverrides(std::nullopt); }

private:
  Viewer2DPanel &panel_;
};

// Mirrors a rendered RGBA symbol image horizontally in-place.
void MirrorImageHorizontally(symbols::RenderedSymbolImage &render) {
  if (render.width <= 0 || render.height <= 0)
    return;
  for (int y = 0; y < render.height; ++y) {
    for (int x = 0; x < render.width / 2; ++x) {
      const int opposite = render.width - 1 - x;
      const size_t left =
          (static_cast<size_t>(y) * static_cast<size_t>(render.width) +
           static_cast<size_t>(x)) *
          4;
      const size_t right =
          (static_cast<size_t>(y) * static_cast<size_t>(render.width) +
           static_cast<size_t>(opposite)) *
          4;
      for (size_t c = 0; c < 4; ++c)
        std::swap(render.rgba[left + c], render.rgba[right + c]);
    }
  }
}

// Resolves fixture GDTF geometry bounds for aspect-driven symbol viewport
// sizing.
bool TryResolveFixtureBoundsMmForCapture(ConfigManager &cfg,
                                         const SceneModelSymbolTarget &target,
                                         FixtureGeometryBounds &bounds) {
  if (target.kind != SceneModelKind::Fixture || target.uuid.empty())
    return false;
  const auto &fixtures = cfg.GetScene().fixtures;
  const auto fixtureIt = fixtures.find(target.uuid);
  if (fixtureIt == fixtures.end())
    return false;

  gui::fixtures::FixtureGdtfResolution resolution;
  std::string error;
  if (!gui::fixtures::ResolveFixtureGdtfDeterministic(
          fixtureIt->second, cfg.GetScene(), resolution, error))
    return false;

  return ComputeFixtureGeometryBoundsMm(
      resolution.selectedPath, fixtureIt->second.gdtfMode, bounds, error);
}

// Returns the expected projected width/height ratio for a symbol view from
// physical bounds.
float ComputeCaptureAspectForView(const FixtureGeometryBounds &bounds,
                                  symbols::SymbolView view) {
  const float x = std::max(1e-3f, bounds.max[0] - bounds.min[0]);
  const float y = std::max(1e-3f, bounds.max[1] - bounds.min[1]);
  const float z = std::max(1e-3f, bounds.max[2] - bounds.min[2]);
  switch (view) {
  case symbols::SymbolView::Top:
  case symbols::SymbolView::Bottom:
    return x / y;
  case symbols::SymbolView::Front:
    return x / z;
  case symbols::SymbolView::Left:
    return y / z;
  }
  return 1.0f;
}

} // namespace

// Captures all orthographic source images in one non-yielding scene scope.
SceneModelSymbolRenderResult CaptureSceneModelOrthographicRenders(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
    const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options) {
  SceneModelSymbolRenderResult result;
  Viewer2DPanel *capturePanel = renderer.GetPanel();
  if (!capturePanel) {
    result.error = "Could not create 2D capture panel instance.";
    return result;
  }

  ScopedViewer2DCaptureState scopedPanelState(*capturePanel);
  ScopedSingleModelCaptureScene isolatedScene(cfg, target,
                                              options.alignToLocalAxes);
  ScopedFixtureCaptureColor captureColor(
      cfg, target.kind == SceneModelKind::Fixture ? target.uuid : std::string(),
      options.forcedFixtureColor);
  scopedPanelState.CompleteReplacement();
  auto prepareOnExit = scopedPanelState.PrepareOnScopeExit();
  Viewer2DRenderOverrides renderOverrides;
  renderOverrides.darkMode = false;
  renderOverrides.showGrid = false;
  renderOverrides.showRuler = false;
  renderOverrides.drawFixtureLabels = false;
  renderOverrides.forceBottomViewForTopFixtures = false;
  renderOverrides.symbolCaptureRenderProfile = true;
  renderOverrides.symbolCaptureIncludeCoplanarEdges = true;
  ScopedViewer2DRenderOverrides scopedRenderOverrides(*capturePanel,
                                                      renderOverrides);
  renderer.SetViewportSize(options.viewportSize);
  renderer.PrepareForCapture();
  renderer.ApplySymbolCaptureDefaults();
  capturePanel->UpdateScene(true);

  std::vector<unsigned char> warmupPixels;
  int warmupWidth = 0;
  int warmupHeight = 0;
  if (!capturePanel->RenderToRGBA(warmupPixels, warmupWidth, warmupHeight)) {
    result.error = "Could not warm up the fixture symbol capture renderer.";
    return result;
  }

  FixtureGeometryBounds fixtureBounds;
  {
    symbols::ScopedFixtureSymbolPhase phase(
        options.timings, symbols::FixtureSymbolPhase::Bounds);
    if (options.fixtureBoundsOverride) {
      fixtureBounds = *options.fixtureBoundsOverride;
      result.fixtureBoundsMm = fixtureBounds;
    } else if (TryResolveFixtureBoundsMmForCapture(cfg, target,
                                                   fixtureBounds)) {
      result.fixtureBoundsMm = fixtureBounds;
    }
  }

  const auto &requests = symbols::FixtureSymbolCapturePlan();
  result.renders.reserve(requests.size());
  for (const auto &request : requests) {
    symbols::ScopedFixtureSymbolPhase phase(
        options.timings, symbols::FixtureSymbolPhase::Capture);
    if (fixtureBounds.valid) {
      const float aspect =
          ComputeCaptureAspectForView(fixtureBounds, request.symbolView);
      const int base =
          std::max(256, std::max(options.viewportSize.GetWidth(),
                                 options.viewportSize.GetHeight()));
      const int width = std::max(
          256, static_cast<int>(aspect >= 1.0f ? base : base * aspect));
      const int height = std::max(
          256, static_cast<int>(aspect >= 1.0f ? base / aspect : base));
      renderer.SetViewportSize(wxSize(width, height));
    } else {
      renderer.SetViewportSize(options.viewportSize);
    }
    renderOverrides.forceBottomViewForTopFixtures =
        request.forceBottomViewForTopFixtures;
    capturePanel->SetRenderOverrides(renderOverrides);
    capturePanel->SetRenderMode(Viewer2DRenderMode::ByFixtureType);
    Viewer2DView viewerView = Viewer2DView::Top;
    if (request.viewerView == symbols::SymbolCaptureViewerView::Front)
      viewerView = Viewer2DView::Front;
    else if (request.viewerView == symbols::SymbolCaptureViewerView::Side)
      viewerView = Viewer2DView::Side;
    capturePanel->SetView(viewerView);
    capturePanel->FitViewToScene();

    symbols::RenderedSymbolImage render;
    render.view = request.symbolView;
    if (!capturePanel->RenderToRGBA(render.rgba, render.width, render.height) ||
        render.width <= 0 || render.height <= 0) {
      result.error = "Could not capture all orthographic source images from "
                     "the 2D viewer.";
      return result;
    }
    if (request.mirrorHorizontally)
      MirrorImageHorizontally(render);
    result.renders.push_back(std::move(render));
  }
  result.ok = result.renders.size() == requests.size();
  return result;
}

// Vectorizes, simplifies, and validates a complete four-view capture set.
SceneModelSymbolCaptureResult ProcessSceneModelOrthographicRenders(
    std::vector<symbols::RenderedSymbolImage> renders,
    const FixtureGeometryBounds &fixtureBoundsMm,
    symbols::FixtureSymbolTimings *timings) {
  SceneModelSymbolCaptureResult result;
  result.fixtureBoundsMm = fixtureBoundsMm;
  const auto &requests = symbols::FixtureSymbolCapturePlan();
  if (renders.size() != requests.size()) {
    result.error = "Could not generate all symbols from captured views.";
    return result;
  }
  {
    symbols::ScopedFixtureSymbolPhase phase(
        timings, symbols::FixtureSymbolPhase::Vectorization);
    result.symbols =
        symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders);
    for (auto &symbol : result.symbols)
      symbols::SimplifySymbolGeometry(symbol, kSymbolRdpEpsilon);
  }
  result.ok = result.symbols.size() == requests.size();
  if (!result.ok)
    result.error = "Could not generate all symbols from captured views.";
  return result;
}

// Captures and processes all orthographic views through one canonical scope.
SceneModelSymbolCaptureResult CaptureSceneModelOrthographicSymbols(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
    const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options) {
  auto capture =
      CaptureSceneModelOrthographicRenders(renderer, cfg, target, options);
  if (!capture.ok)
    return {false, capture.error};
  return ProcessSceneModelOrthographicRenders(
      std::move(capture.renders), capture.fixtureBoundsMm, options.timings);
}

} // namespace tools
