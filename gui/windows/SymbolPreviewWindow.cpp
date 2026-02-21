#include "windows/SymbolPreviewWindow.h"

#include <algorithm>
#include <cmath>

#include <wx/dcbuffer.h>

namespace {

const symbols::Symbol2D *FindSymbol(const std::vector<symbols::Symbol2D> &symbols,
                                    symbols::SymbolView view) {
  auto it = std::find_if(symbols.begin(), symbols.end(),
                         [view](const symbols::Symbol2D &s) { return s.view == view; });
  return it == symbols.end() ? nullptr : &(*it);
}

} // namespace

SymbolPreviewWindow::SymbolPreviewWindow(wxWindow *parent,
                                         std::vector<symbols::Symbol2D> symbols)
    : wxFrame(parent, wxID_ANY, "Fixture Symbol Preview", wxDefaultPosition,
              wxSize(900, 700), wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT),
      symbols_(std::move(symbols)) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  Bind(wxEVT_PAINT, &SymbolPreviewWindow::OnPaint, this);
}

void SymbolPreviewWindow::OnPaint(wxPaintEvent &WXUNUSED(event)) {
  wxAutoBufferedPaintDC dc(this);
  dc.SetBackground(wxBrush(wxColour(248, 248, 248)));
  dc.Clear();

  wxSize size = GetClientSize();
  const int pad = 14;
  const int cellW = std::max(1, (size.GetWidth() - pad * 3) / 2);
  const int cellH = std::max(1, (size.GetHeight() - pad * 3) / 2);

  const symbols::Symbol2D empty{};
  const symbols::Symbol2D *front = FindSymbol(symbols_, symbols::SymbolView::Front);
  const symbols::Symbol2D *top = FindSymbol(symbols_, symbols::SymbolView::Top);
  const symbols::Symbol2D *left = FindSymbol(symbols_, symbols::SymbolView::Left);
  const symbols::Symbol2D *bottom = FindSymbol(symbols_, symbols::SymbolView::Bottom);

  DrawSymbol(dc, wxRect(pad, pad, cellW, cellH), front ? *front : empty, "Front");
  DrawSymbol(dc, wxRect(pad * 2 + cellW, pad, cellW, cellH), top ? *top : empty, "Top");
  DrawSymbol(dc, wxRect(pad, pad * 2 + cellH, cellW, cellH), left ? *left : empty, "Left");
  DrawSymbol(dc, wxRect(pad * 2 + cellW, pad * 2 + cellH, cellW, cellH),
             bottom ? *bottom : empty, "Bottom");
}

void SymbolPreviewWindow::DrawSymbol(wxDC &dc, const wxRect &rect,
                                     const symbols::Symbol2D &symbol,
                                     const wxString &label) {
  dc.SetPen(wxPen(wxColour(210, 210, 210), 1));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawRectangle(rect);
  dc.DrawText(label, rect.GetTopLeft() + wxPoint(8, 8));

  if (!symbol.bounds.valid)
    return;

  const float w = std::max(1e-3f, symbol.bounds.max.x - symbol.bounds.min.x);
  const float h = std::max(1e-3f, symbol.bounds.max.y - symbol.bounds.min.y);
  const double scale = std::min((rect.GetWidth() - 24.0) / w, (rect.GetHeight() - 40.0) / h);
  const double ox = rect.GetX() + (rect.GetWidth() - w * scale) * 0.5 - symbol.bounds.min.x * scale;
  const double oy = rect.GetY() + (rect.GetHeight() - h * scale) * 0.5 - symbol.bounds.min.y * scale;

  dc.SetBrush(wxBrush(wxColour(220, 224, 232)));
  dc.SetPen(*wxTRANSPARENT_PEN);
  for (const auto &poly : symbol.fill) {
    if (poly.outer.size() < 3)
      continue;
    std::vector<wxPoint> points;
    points.reserve(poly.outer.size());
    for (const auto &p : poly.outer) {
      points.emplace_back(static_cast<int>(std::lround(ox + p.x * scale)),
                          static_cast<int>(std::lround(oy + p.y * scale)));
    }
    dc.DrawPolygon(static_cast<int>(points.size()), points.data());
  }

  dc.SetPen(wxPen(*wxBLACK, static_cast<int>(std::max(1.0f, symbol.strokeWidthPx))));
  for (const auto &line : symbol.strokes) {
    if (line.size() < 2)
      continue;
    std::vector<wxPoint> points;
    points.reserve(line.size());
    for (const auto &p : line)
      points.emplace_back(static_cast<int>(std::lround(ox + p.x * scale)),
                          static_cast<int>(std::lround(oy + p.y * scale)));
    dc.DrawLines(static_cast<int>(points.size()), points.data());
  }
}
