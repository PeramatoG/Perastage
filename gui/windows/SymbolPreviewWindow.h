#pragma once

#include <vector>

#include <wx/frame.h>

#include "symbols/Symbol2D.h"

class SymbolPreviewWindow : public wxFrame {
public:
  SymbolPreviewWindow(wxWindow *parent, std::vector<symbols::Symbol2D> symbols);

private:
  void OnPaint(wxPaintEvent &event);
  void DrawSymbol(wxDC &dc, const wxRect &rect, const symbols::Symbol2D &symbol,
                  const wxString &label);

  std::vector<symbols::Symbol2D> symbols_;
};
