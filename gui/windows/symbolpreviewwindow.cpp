#include "windows/symbolpreviewwindow.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/sizer.h>
#include <wx/settings.h>

#include <algorithm>

namespace {
const symbols::Symbol2D *FindSymbol(const symbols::SymbolCollection &symbols,
                                    symbols::SymbolView view) {
  for (const auto &symbol : symbols) {
    if (symbol.view == view)
      return &symbol;
  }
  return nullptr;
}

void DrawSymbolCell(wxGraphicsContext &gc,
                    const wxRect &cell,
                    const symbols::Symbol2D *symbol,
                    const wxString &label) {
  gc.SetPen(*wxTRANSPARENT_PEN);
  gc.SetBrush(wxBrush(wxColour(245, 245, 245)));
  gc.DrawRectangle(cell.GetX(), cell.GetY(), cell.GetWidth(), cell.GetHeight());

  gc.SetPen(wxPen(wxColour(90, 90, 90), 1));
  gc.SetBrush(*wxTRANSPARENT_BRUSH);
  gc.DrawRectangle(cell.GetX() + 0.5, cell.GetY() + 0.5, cell.GetWidth() - 1,
                   cell.GetHeight() - 1);

  gc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT), wxColour(20, 20, 20));
  gc.DrawText(label, cell.GetX() + 8, cell.GetY() + 8);

  if (!symbol || !symbol->bounds.IsValid())
    return;

  const double pad = 18.0;
  const double bodyX = cell.GetX() + pad;
  const double bodyY = cell.GetY() + pad + 18.0;
  const double bodyW = cell.GetWidth() - 2.0 * pad;
  const double bodyH = cell.GetHeight() - 2.0 * pad - 18.0;
  if (bodyW <= 0.0 || bodyH <= 0.0)
    return;

  const double sx = bodyW / std::max(1.0f, symbol->bounds.Width());
  const double sy = bodyH / std::max(1.0f, symbol->bounds.Height());
  const double scale = std::min(sx, sy);
  const double tx = bodyX + (bodyW - symbol->bounds.Width() * scale) * 0.5 -
                    symbol->bounds.minX * scale;
  const double ty = bodyY + (bodyH - symbol->bounds.Height() * scale) * 0.5 -
                    symbol->bounds.minY * scale;

  gc.PushState();
  gc.Translate(tx, ty);
  gc.Scale(scale, scale);

  gc.SetBrush(wxBrush(wxColour(206, 214, 223)));
  gc.SetPen(*wxTRANSPARENT_PEN);
  for (const auto &poly : symbol->fill) {
    auto path = gc.CreatePath();
    if (poly.outer.empty())
      continue;
    path.MoveToPoint(poly.outer.front().x, poly.outer.front().y);
    for (size_t i = 1; i < poly.outer.size(); ++i)
      path.AddLineToPoint(poly.outer[i].x, poly.outer[i].y);
    path.CloseSubpath();
    for (const auto &hole : poly.holes) {
      if (hole.empty())
        continue;
      path.MoveToPoint(hole.front().x, hole.front().y);
      for (size_t i = 1; i < hole.size(); ++i)
        path.AddLineToPoint(hole[i].x, hole[i].y);
      path.CloseSubpath();
    }
    gc.FillPath(path, wxODDEVEN_RULE);
  }

  const double strokePx = symbol->stroke_width_px <= 0.0f ? 2.0f : symbol->stroke_width_px;
  gc.SetPen(wxPen(*wxBLACK, strokePx / std::max(0.001, scale)));
  for (const auto &line : symbol->strokes) {
    if (line.points.size() < 2)
      continue;
    auto path = gc.CreatePath();
    path.MoveToPoint(line.points.front().x, line.points.front().y);
    for (size_t i = 1; i < line.points.size(); ++i)
      path.AddLineToPoint(line.points[i].x, line.points[i].y);
    if (line.closed)
      path.CloseSubpath();
    gc.StrokePath(path);
  }

  gc.PopState();
}
} // namespace

SymbolPreviewPanel::SymbolPreviewPanel(wxWindow *parent,
                                       symbols::SymbolCollection symbols)
    : wxPanel(parent), symbols_(std::move(symbols)) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  Bind(wxEVT_PAINT, &SymbolPreviewPanel::OnPaint, this);
}

void SymbolPreviewPanel::OnPaint(wxPaintEvent &) {
  wxAutoBufferedPaintDC dc(this);
  dc.Clear();
  auto gc = std::unique_ptr<wxGraphicsContext>(wxGraphicsContext::Create(dc));
  if (!gc)
    return;

  const wxSize sz = GetClientSize();
  const int halfW = sz.GetWidth() / 2;
  const int halfH = sz.GetHeight() / 2;

  DrawSymbolCell(*gc, wxRect(0, 0, halfW, halfH),
                 FindSymbol(symbols_, symbols::SymbolView::Front), "Front");
  DrawSymbolCell(*gc, wxRect(halfW, 0, sz.GetWidth() - halfW, halfH),
                 FindSymbol(symbols_, symbols::SymbolView::Top), "Top");
  DrawSymbolCell(*gc, wxRect(0, halfH, halfW, sz.GetHeight() - halfH),
                 FindSymbol(symbols_, symbols::SymbolView::Left), "Left");
  DrawSymbolCell(*gc,
                 wxRect(halfW, halfH, sz.GetWidth() - halfW, sz.GetHeight() - halfH),
                 FindSymbol(symbols_, symbols::SymbolView::Bottom), "Bottom");
}

SymbolPreviewWindow::SymbolPreviewWindow(wxWindow *parent,
                                         const symbols::SymbolCollection &symbols)
    : wxFrame(parent, wxID_ANY, "Generated Fixture Symbols", wxDefaultPosition,
              wxSize(960, 720), wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT) {
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(new SymbolPreviewPanel(this, symbols), 1, wxEXPAND | wxALL, 8);
  SetSizer(sizer);
}
