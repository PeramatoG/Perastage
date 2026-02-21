#include "windows/SymbolPreviewWindow.h"

#include <algorithm>

#include <wx/bitmap.h>
#include <wx/dcbuffer.h>

SymbolPreviewWindow::SymbolPreviewWindow(wxWindow *parent,
                                         const std::array<wxImage, 4> &images)
    : wxFrame(parent, wxID_ANY, "Fixture Symbol Preview", wxDefaultPosition,
              wxSize(1000, 760), wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT),
      images_(images) {
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

  DrawCell(dc, wxRect(pad, pad, cellW, cellH), images_[0], "Front");
  DrawCell(dc, wxRect(pad * 2 + cellW, pad, cellW, cellH), images_[1], "Top");
  DrawCell(dc, wxRect(pad, pad * 2 + cellH, cellW, cellH), images_[2], "Left");
  DrawCell(dc, wxRect(pad * 2 + cellW, pad * 2 + cellH, cellW, cellH), images_[3],
           "Bottom");
}

void SymbolPreviewWindow::DrawCell(wxDC &dc, const wxRect &cell,
                                   const wxImage &image,
                                   const wxString &label) {
  dc.SetPen(wxPen(wxColour(210, 210, 210), 1));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawRectangle(cell);
  dc.DrawText(label, cell.GetTopLeft() + wxPoint(8, 8));

  if (!image.IsOk())
    return;

  const int innerPad = 10;
  const int topLabel = 24;
  const int availW = std::max(1, cell.GetWidth() - innerPad * 2);
  const int availH = std::max(1, cell.GetHeight() - topLabel - innerPad * 2);

  const double sx = static_cast<double>(availW) / image.GetWidth();
  const double sy = static_cast<double>(availH) / image.GetHeight();
  const double scale = std::min(sx, sy);

  const int drawW = std::max(1, static_cast<int>(image.GetWidth() * scale));
  const int drawH = std::max(1, static_cast<int>(image.GetHeight() * scale));

  const int x = cell.GetX() + (cell.GetWidth() - drawW) / 2;
  const int y = cell.GetY() + topLabel + (cell.GetHeight() - topLabel - drawH) / 2;

  wxImage scaled = image.Scale(drawW, drawH, wxIMAGE_QUALITY_HIGH);
  dc.DrawBitmap(wxBitmap(scaled), x, y, true);
}
