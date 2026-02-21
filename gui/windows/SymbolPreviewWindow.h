#pragma once

#include <optional>
#include <vector>

#include <wx/frame.h>

#include "symbols/Symbol2D.h"

class SymbolPreviewWindow : public wxFrame {
public:
  SymbolPreviewWindow(wxWindow *parent,
                      std::vector<symbols::Symbol2D> symbols);

private:
  struct PreviewCell {
    wxRect rect;
    symbols::SymbolView view = symbols::SymbolView::Top;
    wxString label;
  };

  void OnPaint(wxPaintEvent &event);
  void OnSize(wxSizeEvent &event);
  void OnLeftDown(wxMouseEvent &event);
  void OnRightDown(wxMouseEvent &event);
  void OnContextMenu(wxContextMenuEvent &event);
  void OnExportSelectedView(wxCommandEvent &event);
  void ShowExportMenuAt(const wxPoint &point);
  void DrawCell(wxDC &dc, const wxRect &cell, const symbols::Symbol2D *symbol,
                const wxString &label);
  std::vector<PreviewCell> BuildPreviewCells(const wxSize &size) const;
  std::optional<PreviewCell> FindCellAt(const wxPoint &point) const;

  static const symbols::Symbol2D *FindSymbol(
      const std::vector<symbols::Symbol2D> &symbols, symbols::SymbolView view);
  static wxString ViewLabel(symbols::SymbolView view);

  std::vector<symbols::Symbol2D> symbols_;
  std::optional<symbols::SymbolView> selectedViewForExport_;
};
