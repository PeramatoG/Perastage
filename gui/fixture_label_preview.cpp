#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText
#endif

#include <GL/glew.h>
#include "fixture_label_preview.h"
#include "gl_context_utils.h"
#include <nanovg.h>
#define NANOVG_GL2
#include <nanovg_gl.h>
#include "fixtures/fixture_gdtf_resolution.h"
#include "symbols/fixture_symbol_availability.h"
#include "symbols/project_fixture_symbol_runtime.h"
#include "projectutils.h"
#include "opaque_pass_utils.h"
#include <wx/dcclient.h>
#include <wx/font.h>
#include <wx/stattext.h>
#include <cmath>

namespace {
const int kPreviewAttributes[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 24,
                                  WX_GL_STENCIL_SIZE, 8, 0};
}
FixtureLabelPreview::FixtureLabelPreview(wxWindow *parent, ConfigManager &cfg, wxStaticText &status)
    : wxGLCanvas(parent, wxID_ANY, kPreviewAttributes, wxDefaultPosition, wxSize(300,300)), cfg_(cfg), status_(status) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  const auto root = ProjectUtils::GetResourceRoot();
  try { fonts_.emplace(root); }
  catch (const std::exception &) { error_ = _("Bundled label fonts are unavailable."); }
  Bind(wxEVT_PAINT, &FixtureLabelPreview::Paint, this);
  Bind(wxEVT_SIZE, [this](wxSizeEvent &e) { Refresh(); e.Skip(); });
}
FixtureLabelPreview::~FixtureLabelPreview() {
  if (vg_ && gl_lifecycle::TrySetCurrent(*this, context_.get(), "FixtureLabelPreview", "destroy", false))
    nvgDeleteGL2(vg_);
}
void FixtureLabelPreview::SetContent(const Fixture &fixture, int view,
    const viewer2d::FixtureLabelOverrideMap &styles) {
  const bool resourceChanged = fixture_.uuid != fixture.uuid || fixture_.gdtfSpec != fixture.gdtfSpec || fixture_.gdtfMode != fixture.gdtfMode;
  fixture_ = fixture; styles_ = styles; view_ = view;
  const auto projection = ResolveFixtureSymbolProjection(fixture.transform,
      static_cast<Viewer2DView>(view), cfg_.GetFloat("view2d_top_fixtures_inverted") != 0);
  symbolTransform_ = projection.instanceTransform;
  const auto kind = projection.symbolView;
  std::string error;
  gui::fixtures::FixtureGdtfResolution resolution;
  gui::fixtures::ResolveFixtureGdtfDeterministic(fixture, cfg_.GetScene(), resolution, error);
  std::string source = symbols::BuildProjectFixtureSymbolSource(fixture);
  if (source.empty()) source = resolution.selectedPath;
  if (resourceChanged && !tools::ComputeFixtureGeometryBoundsMm(resolution.selectedPath,
      fixture.gdtfMode, physicalBounds_, error)) {
    physicalBounds_.min = {-100,-100,-100}; physicalBounds_.max = {100,100,100};
  }
  Matrix local = fixture.transform; local.o = {0,0,0};
  bool first = true;
  for (int corner = 0; corner < 8; ++corner) {
    std::array<float,3> p;
    for (int axis = 0; axis < 3; ++axis)
      p[axis] = ((corner & (1 << axis)) ? physicalBounds_.max[axis] : physicalBounds_.min[axis]) / 1000;
    const auto world = TransformPoint(local,p);
    const double x = view == 2 ? -world[1] : world[0];
    const double y = view == 0 ? world[1] : world[2];
    if (first) { bounds_ = {x,y,x,y}; first = false; }
    else { bounds_.minX = std::min(bounds_.minX,x); bounds_.minY = std::min(bounds_.minY,y);
           bounds_.maxX = std::max(bounds_.maxX,x); bounds_.maxY = std::max(bounds_.maxY,y); }
  }
  symbol_ = symbol_cache::LoadUsableFixtureSymbol(source, kind);
  Refresh();
}
void FixtureLabelPreview::Paint(wxPaintEvent &) {
  wxPaintDC dc(this);
  if (!fonts_) { dc.DrawText(error_,10,10); return; }
  if (!context_) {
    context_ = std::make_unique<wxGLContext>(this);
    if (!gl_lifecycle::InitializeGlew(*this, *context_, "FixtureLabelPreview").success) {
      error_ = _("Present; preview unavailable");
      dc.DrawText(error_,10,10); return;
    }
  }
  if (!gl_lifecycle::TrySetCurrent(*this, context_.get(), "FixtureLabelPreview", "paint")) return;
  if (!vg_) {
    vg_ = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg_) { dc.DrawText(_("Present; preview unavailable"),10,10); return; }
    const auto root = ProjectUtils::GetResourceRoot();
    for (const bool bold : {false,true}) {
      const auto path = typography::BundledFontPath(root,bold);
      if (nvgCreateFont(vg_, bold ? "perastage-sans-bold" : "perastage-sans", path.string().c_str()) < 0) {
        error_ = _("Bundled label fonts are unavailable.");
        nvgDeleteGL2(vg_); vg_ = nullptr; dc.DrawText(error_,10,10); return;
      }
    }
  }
  const auto size = GetClientSize();
  const double ratio = GetContentScaleFactor();
  glViewport(0,0,std::lround(size.x*ratio),std::lround(size.y*ratio));
  glClearColor(1,1,1,1); glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  nvgBeginFrame(vg_,size.x,size.y,ratio);
  nvgScissor(vg_,0,0,size.x,size.y);
  double minX = -.1, minY = -.1, maxX = .1, maxY = .1;
  double anchorX = 0, anchorY = 0;
  if (symbol_) {
    bool first = true;
    const auto include = [&](const auto &points) {
      for (const auto &p : points) {
        const double x = (p.x + symbol_->offsetXmm) / 1000;
        const double y = (p.y + symbol_->offsetYmm) / 1000;
        if (first) { minX = maxX = x; minY = maxY = y; first = false; }
        else { minX = std::min(minX,x); minY = std::min(minY,y);
               maxX = std::max(maxX,x); maxY = std::max(maxY,y); }
      }
    };
    for (const auto &p : symbol_->strokes) include(p.points);
    for (const auto &p : symbol_->fills) include(p.points);
    anchorX = (minX + maxX) / 2;
    const bool hang = symbol_->viewKind == SymbolViewKind::Front || symbol_->viewKind == SymbolViewKind::Left;
    anchorY = hang ? maxY : (minY + maxY) / 2;
    minX -= anchorX; maxX -= anchorX; minY -= anchorY; maxY -= anchorY;
  }
  const auto &bounds = bounds_;
  // Estimate legacy group stacking without migrating its saved settings.
  auto previewStyles = styles_;
  std::array<viewer2d::LabelFieldStyle,3> legacy;
  double groupHeight = 0;
  for (int f = 0; f < 3; ++f) {
    legacy[f] = viewer2d::ResolveFieldStyle(cfg_, styles_, fixture_.uuid, view_, f);
    if (!viewer2d::HasFieldPresentation(styles_, fixture_.uuid, view_, f) && legacy[f].visible) {
      const auto &font = legacy[f].bold ? fonts_->bold : fonts_->regular;
      groupHeight += (font.ascent + std::abs(font.descent)) * legacy[f].size / font.unitsPerEm;
    }
  }
  double top = groupHeight / 2;
  for (int f = 0; f < 3; ++f) {
    if (viewer2d::HasFieldPresentation(styles_, fixture_.uuid, view_, f)) continue;
    auto &s = legacy[f];
    const auto &font = s.bold ? fonts_->bold : fonts_->regular;
    const double ascent = font.ascent * s.size / font.unitsPerEm;
    const double height = (font.ascent + std::abs(font.descent)) * s.size / font.unitsPerEm;
    s.y += f * s.size + top - ascent;
    if (s.visible) top -= height;
    previewStyles[fixture_.uuid].fieldStyles[view_][f] = s;
  }
  const auto layout = viewer2d::ComposeFixtureLabels(cfg_, previewStyles, fixture_, view_, bounds, *fonts_);
  double extentX = std::max(.5, std::max(std::abs(bounds.minX),std::abs(bounds.maxX)));
  double extentY = std::max(.5, std::max(std::abs(bounds.minY),std::abs(bounds.maxY)));
  for (const auto &t : layout.text) {
    extentX = std::max(extentX, std::abs(static_cast<double>(t.x)) + t.style.fontSize);
    extentY = std::max(extentY, std::abs(t.y) + t.style.fontSize*1.5);
  }
  const double scale = std::min((size.x-30)/(extentX*2), (size.y-60)/(extentY*2));
  const auto point = [&](double x, double y) {
    return std::array<float,2>{static_cast<float>(size.x/2+x*scale),static_cast<float>(size.y/2-y*scale)};
  };
  nvgStrokeColor(vg_,nvgRGB(60,60,65)); nvgStrokeWidth(vg_,1);
  const auto contour = [&](const auto &points) {
    if (points.size() < 2) return;
    bool first = true;
    for (const auto &v : points) {
      const double x = (v.x+symbol_->offsetXmm)/1000-anchorX;
      const double y = (v.y+symbol_->offsetYmm)/1000-anchorY;
      const auto p = point(symbolTransform_.a*x+symbolTransform_.c*y,
                           symbolTransform_.b*x+symbolTransform_.d*y);
      if (first) { nvgMoveTo(vg_,p[0],p[1]); first = false; }
      else nvgLineTo(vg_,p[0],p[1]);
    }
  };
  if (symbol_) {
    for (const auto &p : symbol_->strokes) { nvgBeginPath(vg_); contour(p.points); nvgStroke(vg_); }
    for (const auto &p : symbol_->fills) {
      nvgBeginPath(vg_); contour(p.points); nvgClosePath(vg_);
      for (const auto &hole : p.holes) { contour(hole); nvgClosePath(vg_); nvgPathWinding(vg_,NVG_HOLE); }
      nvgFillColor(vg_,nvgRGB(60,60,65)); nvgFill(vg_);
    }
  } else {
    const auto a = point(bounds.minX,bounds.maxY), b = point(bounds.maxX,bounds.minY);
    nvgBeginPath(vg_); nvgRect(vg_,a[0],a[1],b[0]-a[0],b[1]-a[1]); nvgStroke(vg_);

  }
  for (const auto &t : layout.text) {
    const auto p = point(t.x,t.y);
    nvgFontFace(vg_,t.style.fontFamily.c_str());
    nvgFontSize(vg_,(t.style.ascent+t.style.descent)*scale);
    nvgTextAlign(vg_,NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    nvgFillColor(vg_,nvgRGBAf(t.style.color.r,t.style.color.g,t.style.color.b,1));
    nvgText(vg_,p[0],p[1],t.text.c_str(),nullptr);
  }
  status_.SetLabel(!layout.diagnostics.empty()
      ? _("Unsupported text: change the fixture attribute before export.")
      : symbol_ ? _("Preview uses the first fixture in this scope.") : _("Symbol unavailable"));
  status_.Wrap(290);
  nvgEndFrame(vg_); SwapBuffers();
}
