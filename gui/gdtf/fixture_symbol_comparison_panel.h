#pragma once

#include <wx/panel.h>
#include <vector>
#include <memory>
#include "symbols/fixture_symbol_preview_model.h"
#include "symbols/PerastageSvgSymbol.h"

class FixtureSymbolComparisonPanel : public wxPanel {
public:
  explicit FixtureSymbolComparisonPanel(wxWindow *parent);
  void SetArchivePath(const std::string &path);
private:
  struct Preview {
    wxPanel *panel = nullptr;
    FixtureSymbolResourceSet set;
    SymbolViewKind view;
    FixtureSymbolPreviewModel model;
    std::shared_ptr<const PerastageSvgSymbolData> data;
  };
  void DrawPreview(wxPaintEvent &event);
  std::vector<Preview> previews_;
};
