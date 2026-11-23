#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText
#endif

#include "composed_fixture_labels.h"
#include "fixture_label_composition.h"
#include "projectutils.h"
#include "logger.h"
#include "performance_metrics.h"
#include "render/fixture_label_screen_projection.h"
#include "viewer3dcontroller_cache_helpers.h"
#include <GL/glew.h>
#ifdef __APPLE__
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif
#include <nanovg.h>
#include <wx/translation.h>
#include "render/opengl_state_guard.h"
#include "nanovg_canvas_text.h"
#include "viewer2d_coordinate_math.h"

namespace {
constexpr const char *kRegularFamily = "perastage-sans";
constexpr const char *kBoldFamily = "perastage-sans-bold";

std::optional<viewer2d::FixtureLabelScreenProjection> BuildLabelProjection(
    const ISelectionContext::ViewFrustumSnapshot &camera, int height,
    Viewer2DView view) {
  const auto project = [&](double x, double y) -> std::optional<std::array<double, 2>> {
    double wx = x, wy = y, wz = 0;
    if (view == Viewer2DView::Front) { wy = 0; wz = y; }
    if (view == Viewer2DView::Side) { wx = 0; wy = -x; wz = y; }
    double sx, sy, depth;
    if (gluProject(wx, wy, wz, camera.model, camera.projection,
                   camera.viewport, &sx, &sy, &depth) != GL_TRUE)
      return std::nullopt;
    return std::array<double, 2>{sx, height - sy};
  };
  const auto origin = project(0, 0);
  const auto horizontal = project(1, 0);
  const auto vertical = project(0, 1);
  if (!origin || !horizontal || !vertical)
    return std::nullopt;
  return viewer2d::FixtureLabelScreenProjection{
      (*origin)[0], (*origin)[1], (*horizontal)[0] - (*origin)[0],
      (*vertical)[1] - (*origin)[1]};
}

} // namespace

void DrawComposedFixtureLabels(ISelectionContext &controller, int width, int height,
    Viewer2DView view, float zoom, const viewer2d::FixtureLabelOverrideMap &styles,
    viewer2d::FixtureLabelCompositionCache &cache,
    const ISelectionContext::ViewFrustumSnapshot &camera,
    const std::unordered_set<std::string> &hiddenLayers,
    const std::unordered_set<std::string> &hiddenFixtureTypes,
    bool cullOffscreen,
    const std::unordered_set<std::string> *provisionalFixtures) {
  auto &cfg = ConfigManager::Get();
  if (cfg.GetScene().fixtures.empty())
    return;
  auto *vg = controller.GetNanoVGContext();
  if (!vg || width <= 0 || height <= 0) return;
  // NanoVG flushes alter GL state also used by picking and selection outlines.
  viewer3d::render::OpenGLStateGuard glState;
  nvgBeginFrame(vg, width, height, 1.0f);
  struct FrameGuard {
    NVGcontext *vg;
    ~FrameGuard() {
      performance::Scope flush(performance::Work::View2DLabelDraw,
                               performance::viewer2DFrameActive);
      nvgEndFrame(vg);
    }
  } frame{vg};
  const bool capture = controller.GetCaptureCanvas() != nullptr;
  const auto root = ProjectUtils::GetResourceRoot();
  try {
    const typography::FontPair fonts(root);
    const auto rasterScale = viewer2d::PrepareDefaultLabelAtlas(vg, fonts);
    const std::array<int, 2> fontIds{
        nvgFindFont(vg, kRegularFamily), nvgFindFont(vg, kBoldFamily)};
    const auto projection = BuildLabelProjection(camera, height, view);
    const bool profile = performance::viewer2DFrameActive;
    unsigned renderedLabels = 0, renderedGlyphs = 0, culledLabels = 0;
    std::vector<std::array<double, 2>> screenPositions;
    const auto point = [view](double x, double y, double z) -> std::array<double, 2> {
      if (view == Viewer2DView::Front) return {x, z};
      if (view == Viewer2DView::Side) return {-y, z};
      return {x, y};
    };
    for (const auto &[uuid, fixture] : cfg.GetScene().fixtures) {
      if (!capture && provisionalFixtures && provisionalFixtures->contains(uuid))
        continue;
      if (!ControllerIsLayerVisibleCached(hiddenLayers, fixture.layerUuid) ||
          hiddenFixtureTypes.contains(fixture.typeName))
        continue;
      const auto origin = point(fixture.transform.o[0] * RENDER_SCALE,
                                fixture.transform.o[1] * RENDER_SCALE, fixture.transform.o[2] * RENDER_SCALE);
      viewer2d::LabelBounds bounds{origin[0], origin[1], origin[0], origin[1]};
      const auto it = controller.GetFixtureBoundsMap().find(uuid);
      if (it != controller.GetFixtureBoundsMap().end()) {
        const auto &b = it->second;
        const auto a = point(b.min[0], b.min[1], b.min[2]);
        const auto c = point(b.max[0], b.max[1], b.max[2]);
        bounds = {std::min(a[0],c[0]), std::min(a[1],c[1]), std::max(a[0],c[0]), std::max(a[1],c[1])};
      }
      const auto &layout = cache.Resolve(cfg, styles, fixture, static_cast<int>(view), bounds, fonts);
      if (layout.text.empty())
        continue;
      if (capture)
        controller.GetCaptureCanvas()->SetSourceKey("label:" + uuid);

      performance::Scope culling(performance::Work::View2DLabelCull, profile);
      std::array<bool, 3> visibleFields{};
      bool anyVisible = false;
      for (std::size_t fieldIndex = 0; fieldIndex < layout.fields.size(); ++fieldIndex) {
        const auto &field = layout.fields[fieldIndex];
        if (!field)
          continue;
        const auto outline = layout.text[field->first].style.outlineWidth;
        // Cover the rasterizer's pixel rounding and the screen-space halo.
        const double padding = 3.0 + typography::ScreenOutlineStrokeWidth(outline) * 0.5;
        const bool visible = !cullOffscreen || !projection ||
            projection->IntersectsViewport(field->inkBounds, padding, width, height);
        visibleFields[fieldIndex] = visible;
        anyVisible |= visible;
      }
      culling.Finish();
      if (!anyVisible) {
        ++culledLabels;
        continue;
      }

      performance::Scope drawing(performance::Work::View2DLabelDraw, profile);
      if (projection)
        screenPositions.resize(layout.text.size());
      for (std::size_t fieldIndex = 0; fieldIndex < layout.fields.size(); ++fieldIndex) {
        if (!visibleFields[fieldIndex] || !projection)
          continue;
        const auto &field = *layout.fields[fieldIndex];
        for (std::size_t i = field.first; i < field.first + field.count; ++i) {
          const auto &text = layout.text[i];
          screenPositions[i] = projection->Project(text.x, text.y);
        }
      }

      bool drewLabel = false;
      for (auto pass : {typography::TextInkPass::Outline, typography::TextInkPass::Fill}) {
        const bool fill = pass == typography::TextInkPass::Fill;
        for (std::size_t fieldIndex = 0; fieldIndex < layout.fields.size(); ++fieldIndex) {
          if (!visibleFields[fieldIndex])
            continue;
          const auto &field = *layout.fields[fieldIndex];
          if (!fill && layout.text[field.first].style.outlineWidth <= 0)
            continue;
          for (std::size_t i = field.first; i < field.first + field.count; ++i) {
            const auto &text = layout.text[i];
            if (fill && capture)
              controller.RecordText(text.x, text.y, text.text, text.style);
            if (!projection || !text.style.glyphOutline || text.style.glyphOutline->path.empty())
              continue;
            const auto &xy = screenPositions[i];
            const bool bold = text.style.fontFamily == kBoldFamily;
            const int font = bold || text.style.fontFamily == kRegularFamily
                ? fontIds[bold] : -1;
            viewer2d::DrawCanvasText(vg, text, xy[0], xy[1],
                                     double{viewer2d::kPixelsPerMeter} * zoom,
                                     pass, 1.0, rasterScale[bold], font);
            if (fill) {
              ++renderedGlyphs;
              drewLabel = true;
            }
          }
        }
      }
      renderedLabels += drewLabel;
    }
    if (profile) {
      performance::Count(performance::Work::View2DLabels, renderedLabels);
      performance::Count(performance::Work::View2DGlyphs, renderedGlyphs);
      performance::Count(performance::Work::View2DCulledLabels, culledLabels);
    }
  } catch (const std::exception &e) {
    Logger::Instance().Log(e.what());
    if (capture && controller.GetCaptureCanvas()) {
      CanvasTextStyle failure;
      failure.fontFamily = "perastage-sans";
      controller.GetCaptureCanvas()->SetSourceKey("label:diagnostic");
      controller.RecordText(0, 0, "[label fonts unavailable]", failure);
    }
    if (controller.GetLabelFont() >= 0) {
      nvgFontFaceId(vg, controller.GetLabelFont()); nvgFontSize(vg, 14);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgFillColor(vg, controller.IsDarkMode() ? nvgRGB(255,255,255) : nvgRGB(0,0,0));
      const auto message = _("Bundled label fonts are unavailable.").ToUTF8();
      nvgText(vg, 10, 10, message.data(), nullptr);
    }
  }
}
