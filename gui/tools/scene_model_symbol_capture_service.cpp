#include "tools/scene_model_symbol_capture_service.h"

#include <array>
#include <string>
#include <unordered_map>
#include <utility>

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

namespace tools {
namespace {

constexpr float kSymbolRdpEpsilon = 1.0f;

class ScopedFloatConfigOverride {
public:
  ScopedFloatConfigOverride(ConfigManager &cfg,
                            std::initializer_list<std::pair<const char *, float>> values)
      : cfg_(cfg) {
    for (const auto &entry : values) {
      const std::string key(entry.first);
      previous_[key] = cfg_.GetFloat(key);
      cfg_.SetFloat(key, entry.second);
    }
  }

  ~ScopedFloatConfigOverride() {
    for (const auto &[key, value] : previous_)
      cfg_.SetFloat(key, value);
  }

private:
  ConfigManager &cfg_;
  std::unordered_map<std::string, float> previous_;
};

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

  ScopedSingleModelSceneOverride isolatedSceneOverride(cfg, target,
                                                       options.alignToLocalAxes);
  ScopedFixtureColorOverride selectedFixtureColorOverride(
      cfg, target.kind == SceneModelKind::Fixture ? target.uuid : std::string(),
      options.forcedFixtureColor);
  ScopedFloatConfigOverride displayOverride(
      cfg,
      {
          {"grid_show", 0.0f},
          {"view2d_dark_mode", 0.0f},
          {"view2d_render_mode", static_cast<float>(Viewer2DRenderMode::ByFixtureType)},
          {"label_show_name_top", 0.0f},
          {"label_show_name_front", 0.0f},
          {"label_show_name_side", 0.0f},
          {"label_show_id_top", 0.0f},
          {"label_show_id_front", 0.0f},
          {"label_show_id_side", 0.0f},
          {"label_show_dmx_top", 0.0f},
          {"label_show_dmx_front", 0.0f},
          {"label_show_dmx_side", 0.0f},
      });

  const Viewer2DView previousView = capturePanel->GetView();

  renderer.SetViewportSize(options.viewportSize);
  renderer.PrepareForCapture();
  capturePanel->SetRenderMode(Viewer2DRenderMode::ByFixtureType);
  capturePanel->UpdateScene(true);

  {
    std::vector<unsigned char> warmupPixels;
    int warmupWidth = 0;
    int warmupHeight = 0;
    (void)capturePanel->RenderToRGBA(warmupPixels, warmupWidth, warmupHeight);
  }

  struct CaptureRequest {
    Viewer2DView view = Viewer2DView::Top;
    float topFixturesInverted = 0.0f;
    bool mirrorSideHorizontally = false;
    symbols::SymbolView symbolView = symbols::SymbolView::Top;
  };

  const std::array<CaptureRequest, 4> requests = {
      CaptureRequest{Viewer2DView::Front, 0.0f, false, symbols::SymbolView::Front},
      CaptureRequest{Viewer2DView::Top, 0.0f, false, symbols::SymbolView::Top},
      CaptureRequest{Viewer2DView::Side, 0.0f, true, symbols::SymbolView::Left},
      CaptureRequest{Viewer2DView::Top, 1.0f, false, symbols::SymbolView::Bottom},
  };

  std::vector<symbols::RenderedSymbolImage> renders;
  renders.reserve(requests.size());

  for (const auto &request : requests) {
    ScopedFloatConfigOverride topViewOverride(
        cfg, {{"view2d_top_fixtures_inverted", request.topFixturesInverted}});

    capturePanel->SetView(request.view);
    capturePanel->UpdateScene(true);
    capturePanel->FitViewToScene();

    symbols::RenderedSymbolImage render;
    render.view = request.symbolView;
    const bool ok = capturePanel->RenderToRGBA(render.rgba, render.width, render.height);
    if (!ok || render.width <= 0 || render.height <= 0) {
      capturePanel->SetView(previousView);
      result.error = "Could not capture all orthographic source images from the 2D viewer.";
      return result;
    }
    if (request.mirrorSideHorizontally)
      MirrorImageHorizontally(render);
    renders.push_back(std::move(render));
  }

  capturePanel->SetView(previousView);

  auto symbols = symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders);
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
