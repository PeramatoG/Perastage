#include "windows/SymbolPreviewWindow.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>

#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>

namespace {

void DrawPolyline(wxDC &dc, const symbols::Polyline2D &line, double minX, double minY,
                  double scale, int offsetX, int offsetY, int imageHeight) {
  if (line.size() < 2)
    return;
  std::vector<wxPoint> points;
  points.reserve(line.size());
  for (const auto &p : line) {
    const double px = (static_cast<double>(p.x) - minX) * scale;
    const double py = (static_cast<double>(imageHeight) - static_cast<double>(p.y) - minY) * scale;
    points.emplace_back(offsetX + static_cast<int>(std::round(px)),
                        offsetY + static_cast<int>(std::round(py)));
  }
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
}

const symbols::Symbol2D *SymbolPreviewWindow::FindSymbol(
    const std::vector<symbols::Symbol2D> &symbols, symbols::SymbolView view) {
  auto it = std::find_if(symbols.begin(), symbols.end(),
                         [view](const symbols::Symbol2D &symbol) { return symbol.view == view; });
  return it == symbols.end() ? nullptr : &(*it);
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
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawRectangle(cell);
  dc.DrawText(label, cell.GetTopLeft() + wxPoint(8, 8));

  if (!symbol || !symbol->bounds.valid)
    return;

  const int innerPad = 12;
  const int topLabel = 24;
  const int availW = std::max(1, cell.GetWidth() - innerPad * 2);
  const int availH = std::max(1, cell.GetHeight() - topLabel - innerPad * 2);

  const double symbolW = std::max(1.0, static_cast<double>(symbol->bounds.max.x - symbol->bounds.min.x));
  const double symbolH = std::max(1.0, static_cast<double>(symbol->bounds.max.y - symbol->bounds.min.y));
  const double scale = std::min(static_cast<double>(availW) / symbolW,
                                static_cast<double>(availH) / symbolH);

  const int drawW = std::max(1, static_cast<int>(std::round(symbolW * scale)));
  const int drawH = std::max(1, static_cast<int>(std::round(symbolH * scale)));
  const int x = cell.GetX() + (cell.GetWidth() - drawW) / 2;
  const int y = cell.GetY() + topLabel + (cell.GetHeight() - topLabel - drawH) / 2;

  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(wxColour(225, 230, 238)));
  for (const auto &polygon : symbol->fill) {
    if (polygon.outer.size() < 3)
      continue;
    std::vector<wxPoint> pts;
    pts.reserve(polygon.outer.size());
    for (const auto &p : polygon.outer) {
      const double px = (static_cast<double>(p.x) - symbol->bounds.min.x) * scale;
      const double py = (symbolH - (static_cast<double>(p.y) - symbol->bounds.min.y)) * scale;
      pts.emplace_back(x + static_cast<int>(std::round(px)),
                       y + static_cast<int>(std::round(py)));
    }
    dc.DrawPolygon(static_cast<int>(pts.size()), pts.data());
  }

  dc.SetPen(wxPen(*wxBLACK, std::max(1, static_cast<int>(std::round(symbol->strokeWidthPx)))));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  for (const auto &line : symbol->strokes)
    DrawPolyline(dc, line, symbol->bounds.min.x, symbol->bounds.min.y, scale, x, y,
                 static_cast<int>(std::ceil(symbolH)));
}
