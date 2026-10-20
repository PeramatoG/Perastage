#pragma once
#include "fixture_label_composition.h"
#include "symbols/PerastageSvgSymbol.h"
#include "tools/fixture_geometry_bounds.h"
#include <wx/glcanvas.h>
struct NVGcontext;
class wxStaticText;
#include <memory>
class FixtureLabelPreview : public wxGLCanvas {
public:
  FixtureLabelPreview(wxWindow *parent, ConfigManager &cfg, wxStaticText &status);
  ~FixtureLabelPreview() override;
  void SetContent(const Fixture &fixture, int view,
                  const viewer2d::FixtureLabelOverrideMap &styles);
private:
  void Paint(wxPaintEvent &);
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
};
