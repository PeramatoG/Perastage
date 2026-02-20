#pragma once

#include "symbols/Symbol2DTypes.h"
#include "symboltools/symbol_from_viewer2d.h"

#include <vector>

#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/string.h>

class SymbolPreviewPanel : public wxPanel {
public:
  SymbolPreviewPanel(wxWindow *parent, symbols::SymbolCollection symbols,
                     std::vector<symboltools::SymbolReferenceViews> references);

private:
  void OnPaint(wxPaintEvent &event);

  symbols::SymbolCollection symbols_;
  std::vector<symboltools::SymbolReferenceViews> references_;
};

class SymbolPreviewWindow : public wxFrame {
public:
  SymbolPreviewWindow(wxWindow *parent,
                      const symbols::SymbolCollection &symbols,
                      const std::vector<wxString> &generationLog,
                      const std::vector<symboltools::SymbolReferenceViews> &references);
};
