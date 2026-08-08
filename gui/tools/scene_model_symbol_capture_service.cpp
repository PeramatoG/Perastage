#include "tools/scene_model_symbol_capture_service.h"

#include <array>
#include <chrono>
#include <string>

#include "configmanager.h"
#include "fixture.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "matrixutils.h"
#include "tools/fixture_geometry_bounds.h"
#include "sceneobject.h"
#include "scenedatamanager.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "symbols/SymbolGeometrySimplifier.h"
#include "truss.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"

namespace tools {
namespace {

constexpr float kSymbolRdpEpsilon = 1.0f;

// Copies one model into an immutable capture-only scene snapshot.
SceneDataManager::SceneSnapshot BuildCaptureSnapshot(
    const ConfigManager &cfg, const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options) {
    SceneDataManager::SceneSnapshot snapshot;
    const auto &scene = cfg.GetScene();
    auto alignTransform = [](const Matrix &source) {
      const Matrix identity = MatrixUtils::Identity();
      return MatrixUtils::ApplyRotationPreservingScale(source, identity,
                                                       source.o);
    };
    switch (target.kind) {
    case SceneModelKind::Fixture: {
      auto it = scene.fixtures.find(target.uuid);
      if (it != scene.fixtures.end()) {
        Fixture fixture = it->second;
        if (options.alignToLocalAxes)
          fixture.transform = alignTransform(fixture.transform);
        if (options.forcedFixtureColor)
          fixture.visualColorHex = *options.forcedFixtureColor;
        snapshot.fixtures.emplace(it->first, std::move(fixture));
      }
      break;
    }
    case SceneModelKind::Truss: {
      auto it = scene.trusses.find(target.uuid);
      if (it != scene.trusses.end()) {
        Truss truss = it->second;
        if (options.alignToLocalAxes)
          truss.transform = alignTransform(truss.transform);
        snapshot.trusses.emplace(it->first, std::move(truss));
      }
      break;
    }
    case SceneModelKind::SceneObject: {
      auto it = scene.sceneObjects.find(target.uuid);
      if (it != scene.sceneObjects.end()) {
        SceneObject object = it->second;
        if (options.alignToLocalAxes)
          object.transform = alignTransform(object.transform);
        snapshot.sceneObjects.emplace(it->first, std::move(object));
      }
      break;
    }
    }
    return snapshot;
}

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

// Resolves fixture GDTF geometry bounds for aspect-driven symbol viewport sizing.
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

// Captures top/front/side/bottom orthographic renders and converts them into
// vector symbols.
SceneModelSymbolCaptureResult CaptureSceneModelOrthographicSymbols(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
                                     const SceneModelSymbolTarget &target,
                                     const SceneModelSymbolCaptureOptions &options) {
  SceneModelSymbolCaptureResult result;

  Viewer2DPanel *capturePanel = renderer.GetPanel();
  if (!capturePanel) {
    result.error = "Could not create 2D capture panel instance.";
    return result;
  }

  ScopedViewer2DCaptureState scopedPanelState(*capturePanel);
  const SceneDataManager::SceneSnapshot captureSnapshot =
      BuildCaptureSnapshot(cfg, target, options);
  SceneDataManager::ScopedSnapshot isolatedScene(captureSnapshot);
  capturePanel->PrepareForSceneReplacement();
  capturePanel->CompleteSceneReplacement();
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

  {
    std::vector<unsigned char> warmupPixels;
    int warmupWidth = 0;
    int warmupHeight = 0;
    (void)capturePanel->RenderToRGBA(warmupPixels, warmupWidth, warmupHeight);
  }

  const auto &requests = symbols::FixtureSymbolCapturePlan();

  std::vector<symbols::RenderedSymbolImage> renders;
  renders.reserve(requests.size());
  FixtureGeometryBounds fixtureBounds;
  bool hasFixtureBounds = false;
  {
    symbols::ScopedFixtureSymbolPhase phase(options.timings,
                                            symbols::FixtureSymbolPhase::Bounds);
    hasFixtureBounds = TryResolveFixtureBoundsMmForCapture(cfg, target, fixtureBounds);
  }
  if (hasFixtureBounds)
    result.fixtureBoundsMm = fixtureBounds;

  for (const auto &request : requests) {
    symbols::ScopedFixtureSymbolPhase phase(options.timings,
                                            symbols::FixtureSymbolPhase::Capture);
    if (hasFixtureBounds) {
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
    const bool ok =
        capturePanel->RenderToRGBA(render.rgba, render.width, render.height);
    if (!ok || render.width <= 0 || render.height <= 0) {
      result.error = "Could not capture all orthographic source images from "
                     "the 2D viewer.";
      return result;
    }
    if (request.mirrorHorizontally)
      MirrorImageHorizontally(render);
    renders.push_back(std::move(render));
  }

  std::vector<symbols::Symbol2D> generatedSymbols;
  {
    symbols::ScopedFixtureSymbolPhase phase(
        options.timings, symbols::FixtureSymbolPhase::Vectorization);
    generatedSymbols = symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders);
    for (auto &symbol : generatedSymbols)
      symbols::SimplifySymbolGeometry(symbol, kSymbolRdpEpsilon);
  }
  if (generatedSymbols.size() != requests.size()) {
    result.error = "Could not generate all symbols from captured views.";
    return result;
  }

  result.ok = true;
  result.symbols = std::move(generatedSymbols);
  return result;
}

} // namespace tools
