#include "windows/symbol_preview_drawing.h"

namespace symbol_preview {
void DrawTransparencyBackground(wxDC &dc, const wxRect &rect) {
  constexpr int tile = 10;
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetClippingRegion(rect);
  for (int y = 0; y < rect.GetHeight(); y += tile) {
    for (int x = 0; x < rect.GetWidth(); x += tile) {
      const int shade = ((x / tile + y / tile) % 2) ? 244 : 255;
      dc.SetBrush(wxBrush(wxColour(shade, shade, shade)));
      dc.DrawRectangle(rect.GetX() + x, rect.GetY() + y, tile, tile);
    }
  }
  dc.DestroyClippingRegion();
}
} // namespace symbol_preview
