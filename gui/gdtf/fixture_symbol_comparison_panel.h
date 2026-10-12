#pragma once

#include <wx/panel.h>
#include <vector>
#include <memory>
#include "symbols/fixture_symbol_preview_model.h"
#include "symbols/PerastageSvgSymbol.h"
#include "symbols/gdtf_model_svg_inspection.h"
#include "gdtf/gdtf_resource_bitmap_cache.h"

class wxChoice;
class wxStaticText;

class FixtureSymbolComparisonPanel : public wxPanel {
public:
  explicit FixtureSymbolComparisonPanel(wxWindow *parent);
  void SetArchivePath(const std::string &path, const std::string &projectSource = {});
private:
  struct Preview {
    wxPanel *panel = nullptr;
    FixtureSymbolResourceSet set;
    SymbolViewKind view;
    FixtureSymbolPreviewModel model;
    std::shared_ptr<const PerastageSvgSymbolData> data;
    symbols::ModelSvgResourceInspection standard;
    std::string svg;
    wxString status;
  };
  void DrawPreview(wxPaintEvent &event);
  void UpdateSelectedModel();
  wxChoice *modelSelector_ = nullptr;
  wxStaticText *modelDetails_ = nullptr;
  symbols::GdtfModelSvgCatalog catalog_;
  std::string archivePath_;
  GdtfResourceBitmapCache bitmapCache_;
  std::vector<Preview> previews_;
};
