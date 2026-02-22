#include "windows/SymbolPreviewWindow.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>

#include "windows/symbol_fixture_applier.h"
#include "windows/symbol_preview_exporter.h"

namespace {

constexpr int ID_ExportSelectedViewAsSvg = wxID_HIGHEST + 240;
constexpr int ID_ApplySymbolToFixture = wxID_HIGHEST + 241;

wxPoint ToScreenPoint(const symbols::Point2D &p, const symbols::Aabb2D &bounds,
                      double scale, int originX, int originY) {
  const double sx = (static_cast<double>(p.x) - static_cast<double>(bounds.min.x)) *
                    scale;
  const double sy =
      (static_cast<double>(bounds.max.y) - static_cast<double>(p.y)) * scale;
  return wxPoint(originX + static_cast<int>(std::round(sx)),
                 originY + static_cast<int>(std::round(sy)));
}

void DrawPolyline(wxDC &dc, const symbols::Polyline2D &line,
                  const symbols::Aabb2D &bounds, double scale, int originX,
                  int originY) {
  if (line.size() < 2)
    return;

  std::vector<wxPoint> points;
  points.reserve(line.size());
  for (const auto &p : line)
    points.push_back(ToScreenPoint(p, bounds, scale, originX, originY));

  dc.DrawLines(static_cast<int>(points.size()), points.data());
}

} // namespace

SymbolPreviewWindow::SymbolPreviewWindow(wxWindow *parent,
                                         std::vector<symbols::Symbol2D> symbols,
                                         std::string fixtureUuid)
    : wxFrame(parent, wxID_ANY, "Fixture Symbol Preview", wxDefaultPosition,
              wxSize(1000, 760), wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT),
      symbols_(std::move(symbols)), fixtureUuid_(std::move(fixtureUuid)) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  Bind(wxEVT_PAINT, &SymbolPreviewWindow::OnPaint, this);
  Bind(wxEVT_SIZE, &SymbolPreviewWindow::OnSize, this);
  Bind(wxEVT_CONTEXT_MENU, &SymbolPreviewWindow::OnContextMenu, this);
  Bind(wxEVT_MENU, &SymbolPreviewWindow::OnExportSelectedView, this,
       ID_ExportSelectedViewAsSvg);
  Bind(wxEVT_MENU, &SymbolPreviewWindow::OnApplySymbolToFixture, this,
       ID_ApplySymbolToFixture);
}

const symbols::Symbol2D *SymbolPreviewWindow::FindSymbol(
    const std::vector<symbols::Symbol2D> &symbols, symbols::SymbolView view) {
  auto it =
      std::find_if(symbols.begin(), symbols.end(), [view](const symbols::Symbol2D &symbol) {
        return symbol.view == view;
      });
  return it == symbols.end() ? nullptr : &(*it);
}

wxString SymbolPreviewWindow::ViewLabel(symbols::SymbolView view) {
  switch (view) {
  case symbols::SymbolView::Front:
    return "Front";
  case symbols::SymbolView::Top:
    return "Top";
  case symbols::SymbolView::Bottom:
    return "Bottom";
  case symbols::SymbolView::Left:
    return "Left";
  }
  return "View";
}

std::vector<SymbolPreviewWindow::PreviewCell>
SymbolPreviewWindow::BuildPreviewCells(const wxSize &size) const {
  const int pad = 14;
  const int cellW = std::max(1, (size.GetWidth() - pad * 3) / 2);
  const int cellH = std::max(1, (size.GetHeight() - pad * 3) / 2);

  return {
      {wxRect(pad, pad, cellW, cellH), symbols::SymbolView::Front, "Front"},
      {wxRect(pad * 2 + cellW, pad, cellW, cellH), symbols::SymbolView::Top, "Top"},
      {wxRect(pad, pad * 2 + cellH, cellW, cellH), symbols::SymbolView::Left, "Left"},
      {wxRect(pad * 2 + cellW, pad * 2 + cellH, cellW, cellH),
       symbols::SymbolView::Bottom, "Bottom"},
  };
}

std::optional<SymbolPreviewWindow::PreviewCell>
SymbolPreviewWindow::FindCellAt(const wxPoint &point) const {
  for (const auto &cell : BuildPreviewCells(GetClientSize())) {
    if (cell.rect.Contains(point))
      return cell;
  }
  return std::nullopt;
}

void SymbolPreviewWindow::OnSize(wxSizeEvent &event) {
  Refresh(false);
  event.Skip();
}

void SymbolPreviewWindow::ShowExportMenuAt(const wxPoint &point) {
  const auto cell = FindCellAt(point);
  if (!cell.has_value())
    return;

  const symbols::Symbol2D *symbol = FindSymbol(symbols_, cell->view);
  if (!symbol || !symbol->bounds.valid) {
    wxMessageBox("This view has no drawable symbol to export.",
                 "Fixture Symbol Preview", wxOK | wxICON_INFORMATION, this);
    return;
  }

  selectedViewForExport_ = cell->view;
  wxMenu menu;
  menu.Append(ID_ExportSelectedViewAsSvg, "Export this view as SVG...");
  menu.Append(ID_ApplySymbolToFixture, "Apply symbol to fixture...");
  PopupMenu(&menu, point);
}

void SymbolPreviewWindow::OnContextMenu(wxContextMenuEvent &event) {
  wxPoint screenPoint = event.GetPosition();
  wxPoint clientPoint = screenPoint;
  if (screenPoint == wxDefaultPosition)
    clientPoint = ScreenToClient(wxGetMousePosition());
  else
    clientPoint = ScreenToClient(screenPoint);

  ShowExportMenuAt(clientPoint);
}

void SymbolPreviewWindow::OnExportSelectedView(wxCommandEvent &WXUNUSED(event)) {
  if (!selectedViewForExport_.has_value())
    return;

  const symbols::SymbolView view = selectedViewForExport_.value();
  const symbols::Symbol2D *symbol = FindSymbol(symbols_, view);
  if (!symbol || !symbol->bounds.valid)
    return;

  const wxString viewLabel = ViewLabel(view);
  const wxString suggestedName =
      "fixture_symbol_" + viewLabel.Lower() + ".svg";
  wxFileDialog saveDialog(this, "Export Symbol View as SVG", wxEmptyString,
                          suggestedName, "SVG files (*.svg)|*.svg",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDialog.ShowModal() != wxID_OK)
    return;

  std::string errorMessage;
  if (!symbol_preview::ExportSymbolToSvg(*symbol,
                                         saveDialog.GetPath().ToStdString(),
                                         errorMessage)) {
    wxMessageBox(errorMessage, "Export Symbol View", wxOK | wxICON_ERROR, this);
    return;
  }

  wxMessageBox("Symbol view exported successfully.", "Export Symbol View",
               wxOK | wxICON_INFORMATION, this);
}


void SymbolPreviewWindow::OnApplySymbolToFixture(wxCommandEvent &WXUNUSED(event)) {
  std::string errorMessage;
  if (!symbol_preview::ApplySymbolsToFixtureGdtf(symbols_, fixtureUuid_, errorMessage)) {
    wxMessageBox(errorMessage, "Apply Symbol to Fixture", wxOK | wxICON_ERROR,
                 this);
    return;
  }

  wxMessageBox("Symbol views applied to fixture GDTF successfully.",
               "Apply Symbol to Fixture", wxOK | wxICON_INFORMATION, this);
}

void SymbolPreviewWindow::OnPaint(wxPaintEvent &WXUNUSED(event)) {
  wxAutoBufferedPaintDC dc(this);
  dc.SetBackground(wxBrush(wxColour(248, 248, 248)));
  dc.Clear();

  for (const auto &cell : BuildPreviewCells(GetClientSize())) {
    DrawCell(dc, cell.rect, FindSymbol(symbols_, cell.view), cell.label);
  }
}

void SymbolPreviewWindow::DrawCell(wxDC &dc, const wxRect &cell,
                                   const symbols::Symbol2D *symbol,
                                   const wxString &label) {
  dc.SetPen(wxPen(wxColour(210, 210, 210), 1));
  dc.SetBrush(*wxWHITE_BRUSH);
  dc.DrawRectangle(cell);
  dc.DrawText(label, cell.GetTopLeft() + wxPoint(8, 8));

  if (!symbol || !symbol->bounds.valid)
    return;

  const int innerPad = 12;
  const int topLabel = 24;
  const wxRect contentRect(cell.GetX() + innerPad, cell.GetY() + topLabel,
                           std::max(1, cell.GetWidth() - innerPad * 2),
                           std::max(1, cell.GetHeight() - topLabel - innerPad));

  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(*wxWHITE_BRUSH);
  dc.DrawRectangle(contentRect);

  const double symbolW =
      std::max(1.0, static_cast<double>(symbol->bounds.max.x - symbol->bounds.min.x));
  const double symbolH =
      std::max(1.0, static_cast<double>(symbol->bounds.max.y - symbol->bounds.min.y));
  const double scale = std::min(static_cast<double>(contentRect.GetWidth()) / symbolW,
                                static_cast<double>(contentRect.GetHeight()) / symbolH);

  const int drawW = std::max(1, static_cast<int>(std::round(symbolW * scale)));
  const int drawH = std::max(1, static_cast<int>(std::round(symbolH * scale)));
  const int originX = contentRect.GetX() + (contentRect.GetWidth() - drawW) / 2;
  const int originY = contentRect.GetY() + (contentRect.GetHeight() - drawH) / 2;

  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(wxColour(224, 224, 224)));
  for (const auto &polygon : symbol->fill) {
    if (polygon.outer.size() < 3)
      continue;

    std::vector<wxPoint> pts;
    pts.reserve(polygon.outer.size());
    for (const auto &p : polygon.outer)
      pts.push_back(
          ToScreenPoint(p, symbol->bounds, scale, originX, originY));

    dc.DrawPolygon(static_cast<int>(pts.size()), pts.data());

    if (!polygon.holes.empty()) {
      dc.SetBrush(*wxWHITE_BRUSH);
      for (const auto &hole : polygon.holes) {
        if (hole.size() < 3)
          continue;
        std::vector<wxPoint> holePts;
        holePts.reserve(hole.size());
        for (const auto &p : hole)
          holePts.push_back(
              ToScreenPoint(p, symbol->bounds, scale, originX, originY));
        dc.DrawPolygon(static_cast<int>(holePts.size()), holePts.data());
      }
      dc.SetBrush(wxBrush(wxColour(224, 224, 224)));
    }
  }

  const int strokePx =
      std::max(1, static_cast<int>(std::round(symbol->strokeWidthPx)));
  dc.SetPen(wxPen(wxColour(0, 0, 0), strokePx, wxPENSTYLE_SOLID));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  for (const auto &line : symbol->strokes)
    DrawPolyline(dc, line, symbol->bounds, scale, originX, originY);
}
