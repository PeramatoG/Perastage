#pragma once

#include <vector>

#include <wx/frame.h>

#include "symbols/Symbol2D.h"

class SymbolPreviewWindow : public wxFrame {
public:
  SymbolPreviewWindow(wxWindow *parent,
                      std::vector<symbols::Symbol2D> symbols);

private:
  void OnPaint(wxPaintEvent &event);
  void DrawCell(wxDC &dc, const wxRect &cell, const symbols::Symbol2D *symbol,
                const wxString &label);
  static const symbols::Symbol2D *FindSymbol(
      const std::vector<symbols::Symbol2D> &symbols, symbols::SymbolView view);

  std::vector<symbols::Symbol2D> symbols_;
};
