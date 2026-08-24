#include "tools/scene_model_symbol_capture_service.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

#include "configmanager.h"
#include "diagnostics/DiagnosticLogger.h"
#include "fixture.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "sceneobject.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "symbols/fixture_symbol_resource_revision.h"
#include "symbols/SymbolGeometrySimplifier.h"
#include "tools/fixture_geometry_bounds.h"
#include "tools/scoped_scene_replacement_lifecycle.h"
#include "tools/scoped_single_model_capture_scene.h"
#include "truss.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"
#include "viewer2dviewfit.h"

namespace tools {
namespace {

constexpr float kSymbolRdpEpsilon = 1.0f;
std::atomic<std::uint64_t> s_captureJobId{0};

// Returns the stable diagnostic name for a capture transform policy.
const char *TransformPolicyName(SymbolCaptureTransformPolicy policy) {
  switch (policy) {
  case SymbolCaptureTransformPolicy::PreserveInstanceTransform:
    return "preserve_instance";
  case SymbolCaptureTransformPolicy::AlignRotationPreserveScale:
    return "align_rotation_preserve_scale";
  case SymbolCaptureTransformPolicy::CanonicalFixtureType:
    return "canonical_fixture_type";
  }
  return "unknown";
}

// Formats a complete scene transform for capture diagnostics.
std::string FormatTransform(const Matrix &matrix) {
  std::ostringstream output;
  output << "u=" << matrix.u[0] << ',' << matrix.u[1] << ',' << matrix.u[2]
         << " v=" << matrix.v[0] << ',' << matrix.v[1] << ',' << matrix.v[2]
         << " w=" << matrix.w[0] << ',' << matrix.w[1] << ',' << matrix.w[2]
         << " o=" << matrix.o[0] << ',' << matrix.o[1] << ',' << matrix.o[2];
  return output.str();
}

// Formats scale, determinant, and orthogonality metrics for a transform basis.
std::string FormatTransformMetrics(const Matrix &matrix) {
  const auto length = [](const std::array<float, 3> &axis) {
    return std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] +
                     axis[2] * axis[2]);
  };
  const auto dot = [](const std::array<float, 3> &left,
                      const std::array<float, 3> &right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
  };
  const float determinant =
      matrix.u[0] * (matrix.v[1] * matrix.w[2] - matrix.v[2] * matrix.w[1]) -
      matrix.v[0] * (matrix.u[1] * matrix.w[2] - matrix.u[2] * matrix.w[1]) +
      matrix.w[0] * (matrix.u[1] * matrix.v[2] - matrix.u[2] * matrix.v[1]);
  std::ostringstream output;
  output << " scale=" << length(matrix.u) << ',' << length(matrix.v) << ','
         << length(matrix.w) << " determinant=" << determinant
         << " basis_dot=" << dot(matrix.u, matrix.v) << ','
         << dot(matrix.u, matrix.w) << ',' << dot(matrix.v, matrix.w);
  return output.str();
}

// Builds a lightweight hash and content bounds for one raw RGBA capture.
std::string BuildRgbaFingerprint(const symbols::RenderedSymbolImage &render) {
  std::uint64_t hash = 1469598103934665603ULL;
  int minX = render.width;
  int minY = render.height;
  int maxX = -1;
  int maxY = -1;
  for (int y = 0; y < render.height; ++y) {
    for (int x = 0; x < render.width; ++x) {
      const size_t offset =
          (static_cast<size_t>(y) * static_cast<size_t>(render.width) + x) * 4;
      if (offset + 3 >= render.rgba.size())
        continue;
      for (size_t channel = 0; channel < 4; ++channel) {
        hash ^= render.rgba[offset + channel];
        hash *= 1099511628211ULL;
      }
      if (render.rgba[offset] < 250 || render.rgba[offset + 1] < 250 ||
          render.rgba[offset + 2] < 250 || render.rgba[offset + 3] < 250) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
      }
    }
  }
  std::ostringstream output;
  output << "size=" << render.width << 'x' << render.height << " content=";
  if (maxX < minX || maxY < minY)
    output << "empty";
  else
    output << minX << ',' << minY << '-' << maxX << ',' << maxY;
  output << " rgba_hash=" << std::hex << hash;
  return output.str();
}

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

// Builds the canonical options shared by automatic and manual fixture symbols.
SceneModelSymbolCaptureOptions BuildFixtureTypeSymbolCaptureOptions(
    std::string diagnosticOrigin) {
  SceneModelSymbolCaptureOptions options;
  options.transformPolicy =
      SymbolCaptureTransformPolicy::CanonicalFixtureType;
  options.diagnosticOrigin = std::move(diagnosticOrigin);
  options.forcedFixtureColor = "#3FA9F5";
  return options;
}

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

  const std::uint64_t captureJobId = ++s_captureJobId;
  std::optional<Fixture> originalFixture;
  if (target.kind == SceneModelKind::Fixture) {
    const auto fixtureIt = cfg.GetScene().fixtures.find(target.uuid);
    if (fixtureIt != cfg.GetScene().fixtures.end())
      originalFixture = fixtureIt->second;
  }
#ifndef NDEBUG
  if (originalFixture) {
    gui::fixtures::FixtureGdtfResolution resolution;
    std::string resolutionError;
    const bool resolved = gui::fixtures::ResolveFixtureGdtfDeterministic(
        *originalFixture, cfg.GetScene(), resolution, resolutionError);
    const symbol_cache::GdtfFileRevision revision =
        symbol_cache::ReadGdtfFileRevision(resolved ? resolution.selectedPath
                                                    : std::string());
    diagnostics::DiagnosticLogger::Debug(
        "fixture_symbol_capture_start job=" + std::to_string(captureJobId) +
        " origin=" + options.diagnosticOrigin + " fixture_uuid=" + target.uuid +
        " fixture_type=\"" + originalFixture->typeName + "\" gdtf=\"" +
        originalFixture->gdtfSpec + "\" physical_gdtf=\"" +
        (resolved ? resolution.selectedPath : std::string("<unresolved>")) +
        "\" revision=\"" + revision.Key() + "\" mode=\"" +
        originalFixture->gdtfMode + "\" policy=" +
        TransformPolicyName(options.transformPolicy) +
        " parent=\"" + originalFixture->parentGroupUuid + "\" has_local=" +
        (originalFixture->hasLocalTransform ? "1" : "0") + " original_" +
        FormatTransform(originalFixture->transform) +
        FormatTransformMetrics(originalFixture->transform) + " local_" +
        FormatTransform(originalFixture->localTransform));
  }
#endif

  ScopedViewer2DCaptureState scopedPanelState(*capturePanel);
  ScopedSingleModelCaptureScene isolatedScene(cfg, target,
                                              options.transformPolicy);
#ifndef NDEBUG
  if (target.kind == SceneModelKind::Fixture) {
    const auto isolatedIt = cfg.GetScene().fixtures.find(target.uuid);
    if (isolatedIt != cfg.GetScene().fixtures.end()) {
      diagnostics::DiagnosticLogger::Debug(
          "fixture_symbol_capture_isolated job=" +
          std::to_string(captureJobId) + " fixture_uuid=" + target.uuid + ' ' +
          FormatTransform(isolatedIt->second.transform) + " parent=\"" +
          isolatedIt->second.parentGroupUuid + "\" has_local=" +
          (isolatedIt->second.hasLocalTransform ? "1" : "0") +
          FormatTransformMetrics(isolatedIt->second.transform));
    }
  }
#endif
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
    bool fittedAuthoritativeBounds = false;
    if (target.kind == SceneModelKind::Fixture && fixtureBounds.valid) {
      Viewer3DBoundingBox boundsMeters;
      for (size_t axis = 0; axis < 3; ++axis) {
        boundsMeters.min[axis] = fixtureBounds.min[axis] * 0.001f;
        boundsMeters.max[axis] = fixtureBounds.max[axis] * 0.001f;
      }
      const Viewer2DViewState viewportState = capturePanel->GetViewState();
      viewer2d::ViewFitResult fit;
      fittedAuthoritativeBounds = viewer2d::ComputeViewFitForBounds(
          boundsMeters, viewerView, viewportState.viewportWidth,
          viewportState.viewportHeight, fit);
      if (fittedAuthoritativeBounds) {
        capturePanel->ApplyViewState(fit.offsetXPixels, fit.offsetYPixels,
                                     fit.zoom, viewerView,
                                     Viewer2DRenderMode::ByFixtureType);
      }
    }
    if (!fittedAuthoritativeBounds)
      capturePanel->FitViewToScene();
#ifndef NDEBUG
    const Viewer2DViewState fittedState = capturePanel->GetViewState();
    diagnostics::DiagnosticLogger::Debug(
        "fixture_symbol_capture_view job=" + std::to_string(captureJobId) +
        " view=" + std::to_string(static_cast<int>(request.symbolView)) +
        " viewport=" + std::to_string(fittedState.viewportWidth) + 'x' +
        std::to_string(fittedState.viewportHeight) + " zoom=" +
        std::to_string(fittedState.zoom) + " offset=" +
        std::to_string(fittedState.offsetPixelsX) + ',' +
        std::to_string(fittedState.offsetPixelsY));
#endif

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
#ifndef NDEBUG
    diagnostics::DiagnosticLogger::Debug(
        "fixture_symbol_capture_rgba job=" + std::to_string(captureJobId) +
        " view=" + std::to_string(static_cast<int>(render.view)) + ' ' +
        BuildRgbaFingerprint(render));
#endif
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
