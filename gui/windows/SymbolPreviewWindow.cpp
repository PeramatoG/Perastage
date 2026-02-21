#include "windows/SymbolPreviewWindow.h"

#include <algorithm>
#include <cmath>

#include <wx/dcbuffer.h>
#include <wx/bitmap.h>

SymbolPreviewWindow::SymbolPreviewWindow(wxWindow *parent, const wxImage &image)
    : wxFrame(parent, wxID_ANY, "Fixture Symbol Preview", wxDefaultPosition,
              wxSize(900, 700), wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT),
      image_(image) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  Bind(wxEVT_PAINT, &SymbolPreviewWindow::OnPaint, this);
}

void SymbolPreviewWindow::OnPaint(wxPaintEvent &WXUNUSED(event)) {
  wxAutoBufferedPaintDC dc(this);
  dc.SetBackground(wxBrush(wxColour(248, 248, 248)));
  dc.Clear();

  if (!image_.IsOk())
    return;

  wxSize size = GetClientSize();
  const int pad = 12;
  const int availW = std::max(1, size.GetWidth() - pad * 2);
  const int availH = std::max(1, size.GetHeight() - pad * 2);

  const double sx = static_cast<double>(availW) / image_.GetWidth();
  const double sy = static_cast<double>(availH) / image_.GetHeight();
  const double scale = std::min(sx, sy);

  const int drawW = std::max(1, static_cast<int>(std::lround(image_.GetWidth() * scale)));
  const int drawH = std::max(1, static_cast<int>(std::lround(image_.GetHeight() * scale)));
  const int x = (size.GetWidth() - drawW) / 2;
  const int y = (size.GetHeight() - drawH) / 2;

  wxImage scaled = image_.Scale(drawW, drawH, wxIMAGE_QUALITY_HIGH);
  dc.DrawBitmap(wxBitmap(scaled), x, y, true);
}
