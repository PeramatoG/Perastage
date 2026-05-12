#include "tools/scene_model_symbol_capture_service.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>

#include "configmanager.h"
#include "fixture.h"
#include "matrixutils.h"
#include "sceneobject.h"
#include "support.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "symbols/SymbolGeometrySimplifier.h"
#include "truss.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"
#include <wx/log.h>

namespace tools {
namespace {

constexpr float kSymbolRdpEpsilon = 1.0f;

class ScopedFixtureColorOverride {
public:
  ScopedFixtureColorOverride(ConfigManager &cfg, const std::string &fixtureUuid,
                             const std::optional<std::string> &forcedHex)
      : cfg_(cfg) {
    if (!forcedHex || fixtureUuid.empty())
      return;
    auto &fixtures = cfg_.GetScene().fixtures;
    auto it = fixtures.find(fixtureUuid);
    if (it == fixtures.end())
      return;
    previous_.emplace_back(it->first, it->second.color);
    it->second.color = *forcedHex;
  }

  ~ScopedFixtureColorOverride() {
    auto &fixtures = cfg_.GetScene().fixtures;
    for (const auto &[uuid, color] : previous_) {
      auto it = fixtures.find(uuid);
      if (it != fixtures.end())
        it->second.color = color;
    }
  }

private:
  ConfigManager &cfg_;
  std::vector<std::pair<std::string, std::string>> previous_;
};

class ScopedSingleModelSceneOverride {
public:
  ScopedSingleModelSceneOverride(ConfigManager &cfg,
                                 const SceneModelSymbolTarget &target,
                                 bool alignToLocalAxes)
      : cfg_(cfg) {
    auto &scene = cfg_.GetScene();
    originalFixtures_ = scene.fixtures;
    originalTrusses_ = scene.trusses;
    originalSceneObjects_ = scene.sceneObjects;
    originalSupports_ = scene.supports;

    scene.fixtures.clear();
    scene.trusses.clear();
    scene.sceneObjects.clear();
    scene.supports.clear();

    switch (target.kind) {
    case SceneModelKind::Fixture: {
      auto it = originalFixtures_.find(target.uuid);
      if (it != originalFixtures_.end()) {
        Fixture fixture = it->second;
        if (alignToLocalAxes)
          fixture.transform = AlignRotationToIdentityKeepingScale(fixture.transform);
        scene.fixtures.emplace(it->first, std::move(fixture));
      }
      break;
    }
    case SceneModelKind::Truss: {
      auto it = originalTrusses_.find(target.uuid);
      if (it != originalTrusses_.end()) {
        Truss truss = it->second;
        if (alignToLocalAxes)
          truss.transform = AlignRotationToIdentityKeepingScale(truss.transform);
        scene.trusses.emplace(it->first, std::move(truss));
      }
      break;
    }
    case SceneModelKind::SceneObject: {
      auto it = originalSceneObjects_.find(target.uuid);
      if (it != originalSceneObjects_.end()) {
        SceneObject object = it->second;
        if (alignToLocalAxes)
          object.transform = AlignRotationToIdentityKeepingScale(object.transform);
        scene.sceneObjects.emplace(it->first, std::move(object));
      }
      break;
    }
    }
  }

  ~ScopedSingleModelSceneOverride() {
    auto &scene = cfg_.GetScene();
    scene.fixtures = std::move(originalFixtures_);
    scene.trusses = std::move(originalTrusses_);
    scene.sceneObjects = std::move(originalSceneObjects_);
    scene.supports = std::move(originalSupports_);
  }

private:
  static Matrix AlignRotationToIdentityKeepingScale(const Matrix &source) {
    Matrix identity = MatrixUtils::Identity();
    return MatrixUtils::ApplyRotationPreservingScale(source, identity, source.o);
  }

  ConfigManager &cfg_;
  std::unordered_map<std::string, Fixture> originalFixtures_;
  std::unordered_map<std::string, Truss> originalTrusses_;
  std::unordered_map<std::string, SceneObject> originalSceneObjects_;
  std::unordered_map<std::string, Support> originalSupports_;
};

class ScopedViewer2DCaptureState {
public:
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

  ~ScopedViewer2DCaptureState() {
    panel_.ApplyViewState(offsetXPixels_, offsetYPixels_, zoom_, view_,
                          renderMode_);
    panel_.SetRenderOverrides(renderOverrides_);
    panel_.SetPreferPerastageSvgSymbolsForLayouts(
        preferPerastageSvgSymbolsForLayouts_);
    panel_.UpdateScene(true);
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

class ScopedViewer2DRenderOverrides {
public:
  ScopedViewer2DRenderOverrides(Viewer2DPanel &panel,
                                const Viewer2DRenderOverrides &overrides)
      : panel_(panel) {
    panel_.SetRenderOverrides(overrides);
  }

  ~ScopedViewer2DRenderOverrides() { panel_.SetRenderOverrides(std::nullopt); }

private:
  Viewer2DPanel &panel_;
};

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

// Builds a stable textual fingerprint to compare symbol captures across repeated runs.
std::string BuildSymbolFingerprint(const std::vector<symbols::Symbol2D> &symbols) {
  std::ostringstream ss;
  ss.setf(std::ios::fixed);
  ss.precision(3);
  for (const auto &symbol : symbols) {
    ss << static_cast<int>(symbol.view) << ':';
    ss << symbol.bounds.min.x << ',' << symbol.bounds.min.y << ','
       << symbol.bounds.max.x << ',' << symbol.bounds.max.y << ';';
    ss << symbol.fill.size() << ',' << symbol.strokes.size() << ';';
    for (const auto &polygon : symbol.fill) {
      ss << 'p' << polygon.outer.size();
      for (const auto &point : polygon.outer)
        ss << ',' << point.x << ',' << point.y;
      ss << ';';
    }
    for (const auto &polyline : symbol.strokes) {
      ss << 'l' << polyline.points.size() << ',' << symbol.strokeWidthPx;
      for (const auto &point : polyline.points)
        ss << ',' << point.x << ',' << point.y;
      ss << ';';
    }
  }
  return ss.str();
}

} // namespace

SceneModelSymbolCaptureResult
CaptureSceneModelOrthographicSymbols(Viewer2DOffscreenRenderer &renderer,
                                     ConfigManager &cfg,
                                     const SceneModelSymbolTarget &target,
                                     const SceneModelSymbolCaptureOptions &options) {
  SceneModelSymbolCaptureResult result;

  Viewer2DPanel *capturePanel = renderer.GetPanel();
  if (!capturePanel) {
    result.error = "Could not create 2D capture panel instance.";
    return result;
  }

  ScopedViewer2DCaptureState scopedPanelState(*capturePanel);
  ScopedSingleModelSceneOverride isolatedSceneOverride(cfg, target,
                                                       options.alignToLocalAxes);
  ScopedFixtureColorOverride selectedFixtureColorOverride(
      cfg, target.kind == SceneModelKind::Fixture ? target.uuid : std::string(),
      options.forcedFixtureColor);
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

  {
    std::vector<unsigned char> warmupPixels;
    int warmupWidth = 0;
    int warmupHeight = 0;
    (void)capturePanel->RenderToRGBA(warmupPixels, warmupWidth, warmupHeight);
  }

  struct CaptureRequest {
    Viewer2DView view = Viewer2DView::Top;
    bool forceBottomViewForTopFixtures = false;
    bool mirrorSideHorizontally = false;
    symbols::SymbolView symbolView = symbols::SymbolView::Top;
  };

  const std::array<CaptureRequest, 4> requests = {
      CaptureRequest{Viewer2DView::Front, false, false, symbols::SymbolView::Front},
      CaptureRequest{Viewer2DView::Top, false, false, symbols::SymbolView::Top},
      CaptureRequest{Viewer2DView::Side, false, true, symbols::SymbolView::Left},
      CaptureRequest{Viewer2DView::Top, true, false, symbols::SymbolView::Bottom},
  };

  std::vector<symbols::RenderedSymbolImage> renders;
  renders.reserve(requests.size());

  for (const auto &request : requests) {
    renderOverrides.forceBottomViewForTopFixtures =
        request.forceBottomViewForTopFixtures;
    capturePanel->SetRenderOverrides(renderOverrides);
    capturePanel->SetRenderMode(Viewer2DRenderMode::ByFixtureType);
    capturePanel->SetView(request.view);
    capturePanel->UpdateScene(true);
    capturePanel->FitViewToScene();

    symbols::RenderedSymbolImage render;
    render.view = request.symbolView;
    const bool ok = capturePanel->RenderToRGBA(render.rgba, render.width, render.height);
    if (!ok || render.width <= 0 || render.height <= 0) {
      result.error = "Could not capture all orthographic source images from the 2D viewer.";
      return result;
    }
    if (request.mirrorSideHorizontally)
      MirrorImageHorizontally(render);
    renders.push_back(std::move(render));
  }

  auto symbols = symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders);
  if (symbols.size() != requests.size()) {
    result.error = "Could not generate all symbols from captured views.";
    return result;
  }

  for (auto &symbol : symbols)
    symbols::SimplifySymbolGeometry(symbol, kSymbolRdpEpsilon);

  wxLogTrace("fixture_symbol_capture",
             "Fixture symbol capture fingerprint for %s: %s",
             target.uuid, BuildSymbolFingerprint(symbols));

  result.ok = true;
  result.symbols = std::move(symbols);
  return result;
}

} // namespace tools
