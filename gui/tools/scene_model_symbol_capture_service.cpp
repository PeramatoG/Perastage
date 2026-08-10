#include "tools/scene_model_symbol_capture_service.h"

#include <array>
#include <chrono>
#include <string>

#include "configmanager.h"
#include "fixture.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "scenedatamanager.h"
#include "sceneobject.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "symbols/SymbolGeometrySimplifier.h"
#include "tools/fixture_geometry_bounds.h"
#include "tools/scene_model_symbol_capture_snapshot.h"
#include "truss.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"

namespace tools {
namespace {

constexpr float kSymbolRdpEpsilon = 1.0f;

// Saves and restores 2D viewer state so capture does not affect interactive UI
// state.
class ScopedViewer2DCaptureState {
public:
  // Stores the current 2D viewer state so capture can run without persisting UI
  // changes.
  explicit ScopedViewer2DCaptureState(Viewer2DPanel &panel) : panel_(panel) {
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
    panel_.UpdateScene(false);
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

  return ComputeFixtureGeometryBoundsMm(resolution.selectedPath, bounds, error);
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

// Captures one bounded warm-up or orthographic view from an isolated snapshot.
SceneModelSymbolCaptureStepResult CaptureSceneModelOrthographicStep(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
    const SceneModelSymbolTarget &target, std::size_t stepIndex,
    const SceneModelSymbolCaptureOptions &options) {
  const SceneDataManager::SceneSnapshot snapshot =
      BuildSceneModelSymbolCaptureSnapshot(cfg.GetScene(), target, options);
  return CaptureSceneModelOrthographicStep(renderer, cfg, target, snapshot,
                                           stepIndex, options);
}

// Captures one view from a caller-owned immutable job snapshot.
SceneModelSymbolCaptureStepResult CaptureSceneModelOrthographicStep(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
    const SceneModelSymbolTarget &target,
    const SceneDataManager::SceneSnapshot &snapshot, std::size_t stepIndex,
    const SceneModelSymbolCaptureOptions &options) {
  SceneModelSymbolCaptureStepResult result;
  Viewer2DPanel *capturePanel = renderer.GetPanel();
  if (!capturePanel) {
    result.error = "Could not create 2D capture panel instance.";
    return result;
  }
  const auto &requests = symbols::FixtureSymbolCapturePlan();
  if (stepIndex > requests.size()) {
    result.error = "Fixture symbol capture step is out of range.";
    return result;
  }

  ScopedViewer2DCaptureState scopedPanelState(*capturePanel);
  capturePanel->PrepareForSceneReplacement();
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
  renderer.PrepareForCapture();
  renderer.ApplySymbolCaptureDefaults();

  FixtureGeometryBounds fixtureBounds;
  {
    symbols::ScopedFixtureSymbolPhase phase(
        options.timings, symbols::FixtureSymbolPhase::Bounds);
    if (options.fixtureBoundsOverride) {
      fixtureBounds = *options.fixtureBoundsOverride;
      result.fixtureBoundsMm = fixtureBounds;
    } else if (TryResolveFixtureBoundsMmForCapture(cfg, target, fixtureBounds)) {
      result.fixtureBoundsMm = fixtureBounds;
    }
  }

  auto renderIsolated = [&](auto &render) {
    return ExecuteSceneModelSymbolCaptureBoundary(
        snapshot, [&](const auto &) {
          capturePanel->CompleteSceneReplacement();
          capturePanel->UpdateScene(true);
          capturePanel->FitViewToScene();
          const bool rendered = capturePanel->RenderToRGBA(
              render.rgba, render.width, render.height);
          capturePanel->PrepareForSceneReplacement();
          return rendered;
        });
  };

  if (stepIndex == 0) {
    renderer.SetViewportSize(options.viewportSize);
    symbols::RenderedSymbolImage warmup;
    result.ok = renderIsolated(warmup);
    if (!result.ok)
      result.error = "Could not warm up the fixture symbol capture renderer.";
    return result;
  }

  const auto &request = requests[stepIndex - 1];
  if (fixtureBounds.valid) {
    const float aspect =
        ComputeCaptureAspectForView(fixtureBounds, request.symbolView);
    const int base = std::max(256, std::max(options.viewportSize.GetWidth(),
                                            options.viewportSize.GetHeight()));
    const int width =
        std::max(256, static_cast<int>(aspect >= 1.0f ? base : base * aspect));
    const int height =
        std::max(256, static_cast<int>(aspect >= 1.0f ? base / aspect : base));
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
  symbols::RenderedSymbolImage render;
  render.view = request.symbolView;
  {
    symbols::ScopedFixtureSymbolPhase phase(
        options.timings, symbols::FixtureSymbolPhase::Capture);
    result.ok = renderIsolated(render);
  }
  if (!result.ok || render.width <= 0 || render.height <= 0) {
    result.ok = false;
    result.error = "Could not capture an orthographic fixture symbol view.";
    return result;
  }
  if (request.mirrorHorizontally)
    MirrorImageHorizontally(render);
  result.image = std::move(render);
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

// Captures all orthographic views through the shared incremental primitives.
SceneModelSymbolCaptureResult CaptureSceneModelOrthographicSymbols(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
    const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options) {
  FixtureGeometryBounds bounds;
  std::vector<symbols::RenderedSymbolImage> renders;
  const auto &requests = symbols::FixtureSymbolCapturePlan();
  for (std::size_t step = 0; step <= requests.size(); ++step) {
    auto capture =
        CaptureSceneModelOrthographicStep(renderer, cfg, target, step, options);
    if (!capture.ok)
      return {false, capture.error};
    if (capture.fixtureBoundsMm.valid)
      bounds = capture.fixtureBoundsMm;
    if (capture.image)
      renders.push_back(std::move(*capture.image));
  }
  return ProcessSceneModelOrthographicRenders(std::move(renders), bounds,
                                               options.timings);
}

} // namespace tools
