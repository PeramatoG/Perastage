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

void DrawComposedFixtureLabels(ISelectionContext &controller, int height,
                              Viewer2DView view, float zoom, bool capture) {
  auto &cfg = ConfigManager::Get();
  const auto styles = viewer2d::LoadFixtureLabelOverrides(cfg);
  bool any = false;
  for (const auto &[uuid, entry] : styles)
    for (const auto &v : entry.fieldStyles) for (const auto &f : v) any |= f.has_value();
  if (!any) return;
  const auto root = ProjectUtils::GetResourceRoot();
  try {
    // Application resources are immutable during a renderer's frame.
    static std::filesystem::path loadedRoot;
    static std::optional<typography::FontPair> fonts;
    if (!fonts || loadedRoot != root) { fonts.emplace(root); loadedRoot = root; }
    auto *vg = controller.GetNanoVGContext();
    if (!vg) return;
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
      const auto layout = viewer2d::ComposeFixtureLabels(cfg, styles, fixture, static_cast<int>(view), bounds, *fonts);
      if (capture && controller.GetCaptureCanvas()) controller.GetCaptureCanvas()->SetSourceKey("label:" + uuid);
      for (const auto &t : layout.text) {
        if (capture && controller.GetCaptureCanvas()) controller.RecordText(t.x, t.y, t.text, t.style);
        double wx = t.x, wy = t.y, wz = 0;
        if (view == Viewer2DView::Front) { wy = 0; wz = t.y; }
        if (view == Viewer2DView::Side) { wx = 0; wy = -t.x; wz = t.y; }
        double sx, sy, sz;
        if (gluProject(wx, wy, wz, model, projection, viewport, &sx, &sy, &sz) != GL_TRUE) continue;
        nvgFontFace(vg, t.style.fontFamily.c_str());
        nvgFontSize(vg, (t.style.ascent + t.style.descent) * 25.f * zoom);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
        nvgFillColor(vg, nvgRGBAf(t.style.color.r,t.style.color.g,t.style.color.b,1));
        nvgText(vg, sx, height - sy, t.text.c_str(), nullptr);
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
    if (auto *vg = controller.GetNanoVGContext(); vg && controller.GetLabelFont() >= 0) {
      nvgFontFaceId(vg, controller.GetLabelFont()); nvgFontSize(vg, 14);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgFillColor(vg, controller.IsDarkMode() ? nvgRGB(255,255,255) : nvgRGB(0,0,0));
      const auto message = _("Bundled label fonts are unavailable.").ToUTF8();
      nvgText(vg, 10, 10, message.data(), nullptr);
    }
  }
}
