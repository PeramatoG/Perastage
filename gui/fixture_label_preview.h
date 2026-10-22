#pragma once
#include "fixture_label_composition.h"
#include "symbols/PerastageSvgSymbol.h"
#include "tools/fixture_geometry_bounds.h"
#include "fixture_label_preview_geometry.h"
#include "mouse_capture_ownership.h"
#include <wx/glcanvas.h>
#include <functional>
struct NVGcontext;
class wxStaticText;
#include <memory>
class FixtureLabelPreview : public wxGLCanvas {
public:
  FixtureLabelPreview(wxWindow *parent, ConfigManager &cfg, wxStaticText &status);
  ~FixtureLabelPreview() override;
  void SetContent(const Fixture &fixture, int view,
                  const viewer2d::FixtureLabelOverrideMap &styles);
  void SetActiveField(int field);
  void SetDarkBackground(bool dark);
  void CancelDrag();
  void ResetFit() { lockedScale_.reset(); Refresh(false); }
  using DragCallback = std::function<void(label_preview::DragPhase, const viewer2d::LabelFieldStyle &)>;
  void SetDragCallback(DragCallback callback) { dragCallback_ = std::move(callback); }
  const viewer2d::FixtureLabelComposition &GetComposition() const { return composition_; }
  const label_preview::Transform &GetPreviewTransform() const { return previewTransform_; }
  bool IsDragging() const { return drag_.Active(); }
  viewer2d::LabelFieldStyle ResolveEditingStyle(const viewer2d::FixtureLabelOverrideMap &styles,
      const std::string &uuid, int view, int field) const;
private:
  void Paint(wxPaintEvent &);
  void RebuildComposition();
  void DrawSymbol(double scale);
  void MouseDown(wxMouseEvent &);
  void MouseMove(wxMouseEvent &);
  void MouseUp(wxMouseEvent &);
  void CaptureLost(wxMouseCaptureLostEvent &);
  bool HitSelected(const wxPoint &) const;
  ConfigManager &cfg_;
  wxStaticText &status_;
  std::unique_ptr<wxGLContext> context_;
  NVGcontext *vg_ = nullptr;
  Fixture fixture_;
  tools::FixtureGeometryBounds physicalBounds_;
  viewer2d::LabelBounds bounds_;
  Transform2D symbolTransform_;
  int view_ = 0;
  viewer2d::FixtureLabelOverrideMap styles_;
  std::shared_ptr<const PerastageSvgSymbolData> symbol_;
  std::optional<typography::FontPair> fonts_;
  wxString error_;
  viewer2d::FixtureLabelComposition composition_;
  std::array<viewer2d::LabelFieldStyle,3> displayedStyles_;
  label_preview::Transform previewTransform_{0,0,0};
  label_preview::Point symbolAnchor_;
  label_preview::Drag drag_;
  ui::MouseCaptureOwner mouseCapture_;
  DragCallback dragCallback_;
  std::optional<double> lockedScale_;
  int activeField_ = 0;
  bool darkBackground_ = false, hover_ = false;
  size_t symbolRevision_ = 0;
};
