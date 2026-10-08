#pragma once

#include <wx/dc.h>
#include <wx/graphics.h>

namespace symbol_preview {
void DrawTransparencyBackground(wxDC &dc, const wxRect &rect);
// Appends a closed ring to one compound path; callers fill it with odd-even semantics.
template <typename Ring, typename Transform>
void AppendPreviewRing(wxGraphicsPath &path, const Ring &ring, Transform transform) {
  if (ring.size() < 3)
    return;
  const auto first = transform(ring.front());
  path.MoveToPoint(first.m_x, first.m_y);
  for (std::size_t i = 1; i < ring.size(); ++i) {
    const auto point = transform(ring[i]);
    path.AddLineToPoint(point.m_x, point.m_y);
  }
  path.CloseSubpath();
}
} // namespace symbol_preview
