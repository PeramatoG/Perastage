#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText
#endif

#include <GL/glew.h>
#include "fixture_label_preview.h"
#include "gl_context_utils.h"
#include "gl_canvas_config.h"
#include <nanovg.h>
#define NANOVG_GL2
#include <nanovg_gl.h>
#include "fixtures/fixture_gdtf_resolution.h"
#include "symbols/fixture_symbol_availability.h"
#include "symbols/project_fixture_symbol_runtime.h"
#include "projectutils.h"
#include "opaque_pass_utils.h"
#include "nanovg_canvas_text.h"
#include <wx/dcclient.h>
#include <wx/font.h>
#include <wx/stattext.h>
#include <cmath>

FixtureLabelPreview::FixtureLabelPreview(wxWindow *parent, ConfigManager &cfg, wxStaticText &status)
    : wxGLCanvas(parent, wxID_ANY, gl_lifecycle::GetStandardCanvasAttributes(), wxDefaultPosition, wxSize(300,300)), cfg_(cfg), status_(status), mouseCapture_(*this,"FixtureLabelPreview") {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  const auto root = ProjectUtils::GetResourceRoot();
  try { fonts_.emplace(root); }
  catch (const std::exception &) { error_ = _("Bundled label fonts are unavailable."); }
  Bind(wxEVT_PAINT, &FixtureLabelPreview::Paint, this);
  Bind(wxEVT_SIZE, [this](wxSizeEvent &e) { CancelDrag(); lockedScale_.reset(); Refresh(false); e.Skip(); });
  Bind(wxEVT_LEFT_DOWN, &FixtureLabelPreview::MouseDown, this);
  Bind(wxEVT_MOTION, &FixtureLabelPreview::MouseMove, this);
  Bind(wxEVT_LEFT_UP, &FixtureLabelPreview::MouseUp, this);
  Bind(wxEVT_MOUSE_CAPTURE_LOST, &FixtureLabelPreview::CaptureLost, this);
  Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
    if (e.GetKeyCode() == WXK_ESCAPE && drag_.Active()) CancelDrag(); else e.Skip();
  });
}
FixtureLabelPreview::~FixtureLabelPreview() {
  if (mouseCapture_.IsOwned()) mouseCapture_.Release("preview destroy");
  if (vg_ && gl_lifecycle::TrySetCurrent(*this, context_.get(), "FixtureLabelPreview", "destroy", false))
    nvgDeleteGL2(vg_);
}
void FixtureLabelPreview::SetContent(const Fixture &fixture, int view,
    const viewer2d::FixtureLabelOverrideMap &styles) {
  const bool resourceChanged = fixture_.uuid != fixture.uuid || fixture_.gdtfSpec != fixture.gdtfSpec || fixture_.gdtfMode != fixture.gdtfMode;
  const bool geometryChanged = resourceChanged || view_ != view || symbolRevision_ != cfg_.GetRenderRevision() ||
      fixture_.transform.u != fixture.transform.u || fixture_.transform.v != fixture.transform.v || fixture_.transform.w != fixture.transform.w;
  if (view_ != view) { CancelDrag(); lockedScale_.reset(); }
  fixture_ = fixture; styles_ = styles; view_ = view;
  if (!geometryChanged) { RebuildComposition(); Refresh(false); return; }
  symbolRevision_ = cfg_.GetRenderRevision();
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
  RebuildComposition(); Refresh(false);
}
void FixtureLabelPreview::Paint(wxPaintEvent &) {
  wxPaintDC dc(this);
  if (!fonts_) { dc.DrawText(error_,10,10); return; }
  if (!context_) {
    context_ = std::make_unique<wxGLContext>(this);
    if (!gl_lifecycle::InitializeGlew(*this, *context_, "FixtureLabelPreview").success) {
      error_ = _("Present; preview unavailable"); dc.DrawText(error_,10,10); return;
    }
  }
  if (!gl_lifecycle::TrySetCurrent(*this,context_.get(),"FixtureLabelPreview","paint")) return;
  if (!vg_) {
    vg_ = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg_) { dc.DrawText(_("Present; preview unavailable"),10,10); return; }
    const auto root = ProjectUtils::GetResourceRoot();
    for (const bool bold : {false,true}) {
      const auto path = typography::BundledFontPath(root,bold);
      if (nvgCreateFont(vg_,bold?"perastage-sans-bold":"perastage-sans",path.string().c_str()) < 0) {
        error_ = _("Bundled label fonts are unavailable.");
        nvgDeleteGL2(vg_); vg_=nullptr; dc.DrawText(error_,10,10); return;
      }
    }
  }
  const auto size = GetClientSize();
  if(size.x<=0 || size.y<=0)return;
  const double ratio=GetContentScaleFactor();
  glViewport(0,0,std::lround(size.x*ratio),std::lround(size.y*ratio));
  const float background=darkBackground_?.10f:1.f;
  glClearColor(background,background,background,1);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  nvgBeginFrame(vg_,size.x,size.y,ratio); nvgScissor(vg_,0,0,size.x,size.y);
  double extentX=std::max(.5,std::max(std::abs(bounds_.minX),std::abs(bounds_.maxX)));
  double extentY=std::max(.5,std::max(std::abs(bounds_.minY),std::abs(bounds_.maxY)));
  for(const auto &field:composition_.fields)if(field){
    extentX=std::max({extentX,std::abs(field->bounds.minX),std::abs(field->bounds.maxX)});
    extentY=std::max({extentY,std::abs(field->bounds.minY),std::abs(field->bounds.maxY)});
  }
  const double scale=lockedScale_.value_or(std::min(std::max(1,size.x-30)/(extentX*2),std::max(1,size.y-30)/(extentY*2)));
  // Property edits keep the same visual frame; view/scope/resize explicitly refit.
  if (!lockedScale_) lockedScale_ = scale;
  previewTransform_={size.x/2.0,size.y/2.0,scale};
  DrawSymbol(scale);
  if((hover_ || drag_.Active()) && composition_.fields[activeField_]){
    const auto &b=composition_.fields[activeField_]->bounds;
    const auto p=previewTransform_.ToScreen(b.minX,b.maxY);
    nvgBeginPath(vg_);nvgRect(vg_,p.x-2,p.y-2,(b.maxX-b.minX)*scale+4,(b.maxY-b.minY)*scale+4);
    nvgStrokeColor(vg_,nvgRGB(45,140,255));nvgStrokeWidth(vg_,drag_.Active()?2:1);nvgStroke(vg_);
  }
  // Finish every halo before any fill so adjacent glyphs cannot erase each other.
  for (auto pass : {typography::TextInkPass::Outline, typography::TextInkPass::Fill})
  for(const auto &text:composition_.text){
    const auto p=previewTransform_.ToScreen(text.x,text.y);
    viewer2d::DrawCanvasText(vg_,text,p.x,p.y,scale,pass,ratio);
  }
  status_.SetLabel(!composition_.diagnostics.empty()
      ? _("Unsupported text: change the fixture attribute before export.")
      : darkBackground_ ? _("Dark preview: symbol contours are lightened for display only.")
      : symbol_ ? _("Preview uses the first fixture in this scope.") : _("Symbol unavailable"));
  status_.Wrap(290); nvgEndFrame(vg_); SwapBuffers();
}
