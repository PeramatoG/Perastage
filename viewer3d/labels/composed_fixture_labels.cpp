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
#include "layer_identity.h"
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

void DrawComposedFixtureLabels(ISelectionContext &controller, int width, int height,
    Viewer2DView view, float zoom, const viewer2d::FixtureLabelOverrideMap &styles,
    viewer2d::FixtureLabelCompositionCache &cache) {
  auto &cfg = ConfigManager::Get();
  bool any = false;
  for (const auto &[uuid, entry] : styles)
    for (const auto &v : entry.fieldStyles) for (const auto &f : v) any |= f.has_value();
  if (!any) return;
  auto *vg = controller.GetNanoVGContext();
  if (!vg || width <= 0 || height <= 0) return;
  // NanoVG flushes alter GL state also used by picking and selection outlines.
  viewer3d::render::OpenGLStateGuard glState;
  nvgBeginFrame(vg, width, height, 1.0f);
  struct FrameGuard {
    NVGcontext *vg;
    ~FrameGuard() { nvgEndFrame(vg); }
  } frame{vg};
  const bool capture = controller.GetCaptureCanvas() != nullptr;
  const auto root = ProjectUtils::GetResourceRoot();
  try {
    // Application resources are immutable during a renderer's frame.
    static std::filesystem::path loadedRoot;
    static std::optional<typography::FontPair> fonts;
    if (!fonts || loadedRoot != root) { fonts.emplace(root); loadedRoot = root; cache.Clear(); }
    for (const bool bold : {false, true}) {
      const char *name = bold ? "perastage-sans-bold" : "perastage-sans";
      if (nvgFindFont(vg, name) < 0) {
        const auto path = typography::BundledFontPath(root, bold);
        if (nvgCreateFont(vg, name, path.string().c_str()) < 0)
          throw std::runtime_error("Cannot load bundled label font");
      }
    }
    double model[16], projection[16]; int viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, model); glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);
    const auto point = [view](double x, double y, double z) -> std::array<double, 2> {
      if (view == Viewer2DView::Front) return {x, z};
      if (view == Viewer2DView::Side) return {-y, z};
      return {x, y};
    };
    for (const auto &[uuid, fixture] : cfg.GetScene().fixtures) {
      if (!cfg.IsLayerVisible(fixture.layerUuid) || !cfg.IsFixtureTypeVisible(fixture.typeName)) continue;
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
      const auto &layout = cache.Resolve(cfg, styles, fixture, static_cast<int>(view), bounds, *fonts);
      if (capture && controller.GetCaptureCanvas()) controller.GetCaptureCanvas()->SetSourceKey("label:" + uuid);
      for (auto pass : {typography::TextInkPass::Outline, typography::TextInkPass::Fill})
      for (const auto &t : layout.text) {
        if (pass == typography::TextInkPass::Fill && capture && controller.GetCaptureCanvas())
          controller.RecordText(t.x, t.y, t.text, t.style);
        double wx = t.x, wy = t.y, wz = 0;
        if (view == Viewer2DView::Front) { wy = 0; wz = t.y; }
        if (view == Viewer2DView::Side) { wx = 0; wy = -t.x; wz = t.y; }
        double sx, sy, sz;
        if (gluProject(wx, wy, wz, model, projection, viewport, &sx, &sy, &sz) != GL_TRUE) continue;
        viewer2d::DrawCanvasText(vg, t, sx, height - sy, 25.0 * zoom, pass);
      }
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
