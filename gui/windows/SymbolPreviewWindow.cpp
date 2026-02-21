#include "windows/SymbolPreviewWindow.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <wx/dcbuffer.h>

namespace {

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
                                         std::vector<symbols::Symbol2D> symbols)
    : wxFrame(parent, wxID_ANY, "Fixture Symbol Preview", wxDefaultPosition,
              wxSize(1000, 760), wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT),
      symbols_(std::move(symbols)) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  Bind(wxEVT_PAINT, &SymbolPreviewWindow::OnPaint, this);
  Bind(wxEVT_SIZE, &SymbolPreviewWindow::OnSize, this);
}

const symbols::Symbol2D *SymbolPreviewWindow::FindSymbol(
    const std::vector<symbols::Symbol2D> &symbols, symbols::SymbolView view) {
  auto it =
      std::find_if(symbols.begin(), symbols.end(), [view](const symbols::Symbol2D &symbol) {
        return symbol.view == view;
      });
  return it == symbols.end() ? nullptr : &(*it);
}

void SymbolPreviewWindow::OnSize(wxSizeEvent &event) {
  Refresh(false);
  event.Skip();
}

void SymbolPreviewWindow::OnPaint(wxPaintEvent &WXUNUSED(event)) {
  wxAutoBufferedPaintDC dc(this);
  dc.SetBackground(wxBrush(wxColour(248, 248, 248)));
  dc.Clear();

  wxSize size = GetClientSize();
  const int pad = 14;
  const int cellW = std::max(1, (size.GetWidth() - pad * 3) / 2);
  const int cellH = std::max(1, (size.GetHeight() - pad * 3) / 2);

  DrawCell(dc, wxRect(pad, pad, cellW, cellH),
           FindSymbol(symbols_, symbols::SymbolView::Front), "Front");
  DrawCell(dc, wxRect(pad * 2 + cellW, pad, cellW, cellH),
           FindSymbol(symbols_, symbols::SymbolView::Top), "Top");
  DrawCell(dc, wxRect(pad, pad * 2 + cellH, cellW, cellH),
           FindSymbol(symbols_, symbols::SymbolView::Left), "Left");
  DrawCell(dc, wxRect(pad * 2 + cellW, pad * 2 + cellH, cellW, cellH),
           FindSymbol(symbols_, symbols::SymbolView::Bottom), "Bottom");
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
