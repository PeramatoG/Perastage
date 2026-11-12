/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_canvas.h"
#include <wx/dc.h>
#include <algorithm>

void DmxPatchCanvas::SetHighlights(std::map<std::string, wxColour> highlights) {
  if (highlights_ == highlights) return;
  highlights_ = std::move(highlights);
  Refresh();
}
void DmxPatchCanvas::PaintHighlights(wxDC &dc, int firstRow, int lastRow) {
  const int w = geometry_.width, h = geometry_.height, columns = geometry_.columns;
  const auto contains = [&](int index, const std::string &uuid) {
    return index >= 0 && index < 512 && dmx_patch_ui::FixtureIds(snapshot_->cells[index]).contains(uuid);
  };
  // Outline row segments rather than every byte. Actual sparse holes remain
  // visible, and each overlapping fixture keeps its own identifiable contour.
  int layer = 0;
  for (const auto &[uuid, color] : highlights_) {
    if (!blocks_->bounds.contains(uuid)) continue;
    const int inset = FromDIP(2 + layer % 3);
    for (int row = firstRow; row < lastRow; ++row) {
      for (int col = 0; col < columns && row * columns + col < 512; ++col) {
        const int index = row * columns + col;
        if (!contains(index, uuid)) continue;
        const bool left = col == 0 || !contains(index - 1, uuid);
        const bool right = col + 1 == columns || !contains(index + 1, uuid);
        const bool start = index == blocks_->bounds.at(uuid).first;
        const bool end = index == blocks_->bounds.at(uuid).second;
        const int x1 = col * w + (left ? inset : 0), x2 = (col + 1) * w - (right ? inset : 0);
        const int y1 = row * h + inset, y2 = (row + 1) * h - inset;
        const int radius = std::max(1, std::min(FromDIP(18), std::min(w, h) / 2 - 1) - inset);
        dc.SetPen(wxPen(color, FromDIP(2)));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawLine(x1 + (start ? radius : 0), y1, x2 - (end ? radius : 0), y1);
        dc.DrawLine(x1, y2, x2, y2);
        if (left) dc.DrawLine(x1, y1 + (start ? radius : 0), x1, y2);
        if (right) dc.DrawLine(x2, y1 + (end ? radius : 0), x2, y2);
        if (start) dc.DrawEllipticArc(x1, y1, 2 * radius, 2 * radius, 90, 180);
        if (end) dc.DrawEllipticArc(x2 - 2 * radius, y1, 2 * radius, 2 * radius, 0, 90);
      }
    }
    ++layer;
  }
}
