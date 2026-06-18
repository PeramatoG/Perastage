#include "tools/scene_model_symbol_capture_service.h"

#include <array>
#include <cfloat>
#include <string>
#include <unordered_map>

#include "configmanager.h"
#include "fixture.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "gdtfloader.h"
#include "matrixutils.h"
#include "sceneobject.h"
#include "support.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "symbols/SymbolGeometrySimplifier.h"
#include "truss.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"

namespace tools {
namespace {

constexpr float kSymbolRdpEpsilon = 1.0f;

struct Bounds3D {
  std::array<float, 3> min = {FLT_MAX, FLT_MAX, FLT_MAX};
  std::array<float, 3> max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  bool valid = false;
};

// Transforms a point by a Matrix using local-space basis vectors and
// translation.
std::array<float, 3> TransformPoint(const Matrix &m,
                                    const std::array<float, 3> &p) {
  return {m.u[0] * p[0] + m.v[0] * p[1] + m.w[0] * p[2] + m.o[0],
          m.u[1] * p[0] + m.v[1] * p[1] + m.w[1] * p[2] + m.o[1],
          m.u[2] * p[0] + m.v[2] * p[1] + m.w[2] * p[2] + m.o[2]};
}

// Expands 3D bounds to include a single transformed point.
void ExtendBounds(Bounds3D &bounds, const std::array<float, 3> &p) {
  bounds.min[0] = std::min(bounds.min[0], p[0]);
  bounds.min[1] = std::min(bounds.min[1], p[1]);
  bounds.min[2] = std::min(bounds.min[2], p[2]);
  bounds.max[0] = std::max(bounds.max[0], p[0]);
  bounds.max[1] = std::max(bounds.max[1], p[1]);
  bounds.max[2] = std::max(bounds.max[2], p[2]);
  bounds.valid = true;
}

// Temporarily overrides a fixture color during symbol capture and restores it
// afterward.
class ScopedFixtureColorOverride {
public:
  // Applies an optional temporary fixture color override for symbol capture.
  ScopedFixtureColorOverride(ConfigManager &cfg, const std::string &fixtureUuid,
                             const std::optional<std::string> &forcedHex)
      : cfg_(cfg) {
    if (!forcedHex || fixtureUuid.empty())
      return;
    auto &fixtures = cfg_.GetScene().fixtures;
    auto it = fixtures.find(fixtureUuid);
    if (it == fixtures.end())
      return;
    previous_.emplace_back(it->first, it->second.visualColorHex);
    it->second.visualColorHex = *forcedHex;
  }

  // Restores any fixture colors overridden during symbol capture.
  ~ScopedFixtureColorOverride() {
    auto &fixtures = cfg_.GetScene().fixtures;
    for (const auto &[uuid, color] : previous_) {
      auto it = fixtures.find(uuid);
      if (it != fixtures.end())
        it->second.visualColorHex = color;
    }
  }

private:
  ConfigManager &cfg_;
  std::vector<std::pair<std::string, std::string>> previous_;
};

// Temporarily isolates the scene to a single model target for deterministic
// capture.
class ScopedSingleModelSceneOverride {
public:
  // Replaces scene content with only the requested model target for isolated
  // capture.
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
          fixture.transform =
              AlignRotationToIdentityKeepingScale(fixture.transform);
        scene.fixtures.emplace(it->first, std::move(fixture));
      }
      break;
    }
    case SceneModelKind::Truss: {
      auto it = originalTrusses_.find(target.uuid);
      if (it != originalTrusses_.end()) {
        Truss truss = it->second;
        if (alignToLocalAxes)
          truss.transform =
              AlignRotationToIdentityKeepingScale(truss.transform);
        scene.trusses.emplace(it->first, std::move(truss));
      }
      break;
    }
    case SceneModelKind::SceneObject: {
      auto it = originalSceneObjects_.find(target.uuid);
      if (it != originalSceneObjects_.end()) {
        SceneObject object = it->second;
        if (alignToLocalAxes)
          object.transform =
              AlignRotationToIdentityKeepingScale(object.transform);
        scene.sceneObjects.emplace(it->first, std::move(object));
      }
      break;
    }
    }
  }

  // Restores the original scene content after isolated capture is complete.
  ~ScopedSingleModelSceneOverride() {
    auto &scene = cfg_.GetScene();
    scene.fixtures = std::move(originalFixtures_);
    scene.trusses = std::move(originalTrusses_);
    scene.sceneObjects = std::move(originalSceneObjects_);
    scene.supports = std::move(originalSupports_);
  }

private:
  // Rebuilds a transform with identity rotation while preserving scale and
  // translation.
  static Matrix AlignRotationToIdentityKeepingScale(const Matrix &source) {
    Matrix identity = MatrixUtils::Identity();
    return MatrixUtils::ApplyRotationPreservingScale(source, identity,
                                                     source.o);
  }

  ConfigManager &cfg_;
  std::unordered_map<std::string, Fixture> originalFixtures_;
  std::unordered_map<std::string, Truss> originalTrusses_;
  std::unordered_map<std::string, SceneObject> originalSceneObjects_;
  std::unordered_map<std::string, Support> originalSupports_;
};

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
                                         Bounds3D &bounds) {
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

  std::vector<GdtfObject> objects;
  if (!LoadGdtf(resolution.selectedPath, objects, &error))
    return false;

  for (const auto &object : objects) {
    if (object.isLens)
      continue;
    for (size_t vi = 0; vi + 2 < object.mesh.vertices.size(); vi += 3) {
      const std::array<float, 3> local = {object.mesh.vertices[vi],
                                          object.mesh.vertices[vi + 1],
                                          object.mesh.vertices[vi + 2]};
      ExtendBounds(bounds, TransformPoint(object.transform, local));
    }
  }
  return bounds.valid;
}

// Returns the expected projected width/height ratio for a symbol view from
// physical bounds.
float ComputeCaptureAspectForView(const Bounds3D &bounds,
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
  ScopedSingleModelSceneOverride isolatedSceneOverride(
      cfg, target, options.alignToLocalAxes);
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
      CaptureRequest{Viewer2DView::Front, false, false,
                     symbols::SymbolView::Front},
      CaptureRequest{Viewer2DView::Top, false, false, symbols::SymbolView::Top},
      CaptureRequest{Viewer2DView::Side, false, true,
                     symbols::SymbolView::Left},
      CaptureRequest{Viewer2DView::Top, true, false,
                     symbols::SymbolView::Bottom},
  };

  std::vector<symbols::RenderedSymbolImage> renders;
  renders.reserve(requests.size());
  Bounds3D fixtureBounds;
  const bool hasFixtureBounds =
      TryResolveFixtureBoundsMmForCapture(cfg, target, fixtureBounds);

  for (const auto &request : requests) {
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
    capturePanel->SetView(request.view);
    capturePanel->UpdateScene(true);
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
    if (request.mirrorSideHorizontally)
      MirrorImageHorizontally(render);
    renders.push_back(std::move(render));
  }

  auto symbols =
      symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders);
  if (symbols.size() != requests.size()) {
    result.error = "Could not generate all symbols from captured views.";
    return result;
  }

  for (auto &symbol : symbols)
    symbols::SimplifySymbolGeometry(symbol, kSymbolRdpEpsilon);

  result.ok = true;
  result.symbols = std::move(symbols);
  return result;
}

} // namespace tools
