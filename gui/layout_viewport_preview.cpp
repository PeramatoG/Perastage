/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "layout_viewport_preview.h"
#include "paper_stroke_mapping.h"
#include "document_text_renderer.h"
#include "typography/font_resources.h"
#include "typography/glyph_outline.h"
#include "projectutils.h"
#include <wx/wx.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>
#include <algorithm>
#include <cmath>
namespace {
wxColour Color(const CanvasColor &v) {
  auto byte = [](float x) { return static_cast<unsigned char>(std::clamp(x, 0.0f, 1.0f) * 255); };
  return {byte(v.r), byte(v.g), byte(v.b), byte(v.a)};
}
// A graphics backend only: command traversal, symbols and all world transforms
// remain owned by Viewer2DCommandRenderer, as in vector output.
class PreviewBackend final : public viewer2d::IViewer2DCommandBackend {
public:
  PreviewBackend(wxGraphicsContext &gc, double strokeScale) : gc_(gc), strokeScale_(strokeScale) {}
  void Stroke(const CanvasStroke &s, double width, const CanvasFill *f = nullptr) {
    if (width > 0)
      gc_.SetPen(gc_.CreatePen(wxGraphicsPenInfo(Color(s.color)).Width(width * strokeScale_)
          .Cap(wxCAP_ROUND).Join(wxJOIN_ROUND)));
    else gc_.SetPen(*wxTRANSPARENT_PEN);
    gc_.SetBrush(f ? wxBrush(Color(f->color)) : *wxTRANSPARENT_BRUSH);
  }
  void DrawLine(const viewer2d::Viewer2DRenderPoint &a, const viewer2d::Viewer2DRenderPoint &b,
      const CanvasStroke &s, double width) override {
    Stroke(s, width); gc_.StrokeLine(a.x, a.y, b.x, b.y);
  }
  void Path(const std::vector<viewer2d::Viewer2DRenderPoint> &p,
      const CanvasStroke &s, double width, const CanvasFill *f, bool closed) {
    if (p.empty()) return;
    Stroke(s, width, f);
    auto path = gc_.CreatePath(); path.MoveToPoint(p.front().x, p.front().y);
    for (size_t i = 1; i < p.size(); ++i) path.AddLineToPoint(p[i].x, p[i].y);
    if (closed) path.CloseSubpath();
    if (f) gc_.FillPath(path);
    if (width > 0) gc_.StrokePath(path);
  }
  void DrawPolyline(const std::vector<viewer2d::Viewer2DRenderPoint> &p,
      const CanvasStroke &s, double width) override { Path(p, s, width, nullptr, false); }
  void DrawPolygon(const std::vector<viewer2d::Viewer2DRenderPoint> &p,
      const CanvasStroke &s, const CanvasFill *f, double width) override { Path(p, s, width, f, true); }
  void DrawCircle(const viewer2d::Viewer2DRenderPoint &p, double radius,
      const CanvasStroke &s, const CanvasFill *f, double width) override {
    Stroke(s, width, f);
    auto path = gc_.CreatePath(); path.AddEllipse(p.x-radius, p.y-radius, 2*radius, 2*radius);
    if (f) gc_.FillPath(path);
    if (width > 0) gc_.StrokePath(path);
  }
  void DrawText(const viewer2d::Viewer2DRenderText &t) override {
    if (t.fontSizePx <= 0) return;
    auto outline = t.style.glyphOutline;
    if (!outline && t.style.fontFamily.starts_with("perastage-sans")) {
      // Persisted command caches retain font identity and positioned glyphs.
      // Recover only their contours through the existing bundled font service.
      static const auto fonts = [] {
        std::array<typography::TtfFontMetrics, 2> result;
        for (int bold = 0; bold < 2; ++bold)
          typography::LoadTtfFontMetrics(typography::BundledFontPath(
              ProjectUtils::GetResourceRoot(), bold != 0), result[bold]);
        return result;
      }();
      const auto encoded = typography::EncodeWinAnsi(t.text);
      if (!encoded.empty()) outline = typography::ReadGlyphOutline(
          fonts[t.style.fontFamily == "perastage-sans-bold"],
          static_cast<unsigned char>(encoded.front()));
    }
    if (outline) {
      const auto path = gui::BuildDocumentGlyphPath(gc_, *outline, t.anchor.x, t.anchor.y, t.fontSizePx);
      gc_.SetBrush(wxBrush(Color(t.style.color)));
      if (t.outlineWidthPx > 0) {
        gc_.SetPen(gc_.CreatePen(wxGraphicsPenInfo(Color(t.style.outlineColor)).Width(2*t.outlineWidthPx)));
        gc_.StrokePath(path);
      }
      gc_.FillPath(path, wxWINDING_RULE);
      return;
    }
    auto font = gc_.CreateFont(t.fontSizePx, wxString::FromUTF8(t.style.fontFamily),
                              wxFONTFLAG_DEFAULT, Color(t.style.color));
    gc_.SetFont(font);
    double w, h, descent, leading;
    const auto value = wxString::FromUTF8(t.text);
    gc_.GetTextExtent(value, &w, &h, &descent, &leading);
    double x = t.anchor.x, y = t.anchor.y;
    if (t.style.hAlign == CanvasTextStyle::HorizontalAlign::Center) x -= w/2;
    if (t.style.hAlign == CanvasTextStyle::HorizontalAlign::Right) x -= w;
    if (t.style.vAlign == CanvasTextStyle::VerticalAlign::Baseline) y -= h-descent;
    if (t.style.vAlign == CanvasTextStyle::VerticalAlign::Middle) y -= h/2;
    if (t.style.vAlign == CanvasTextStyle::VerticalAlign::Bottom) y -= h;
    if (t.outlineWidthPx > 0) {
      gc_.SetFont(gc_.CreateFont(t.fontSizePx, wxString::FromUTF8(t.style.fontFamily),
                               wxFONTFLAG_DEFAULT, Color(t.style.outlineColor)));
      for (int dx : {-1,0,1}) for (int dy : {-1,0,1})
        if (dx || dy) gc_.DrawText(value, x + dx*t.outlineWidthPx, y + dy*t.outlineWidthPx);
      gc_.SetFont(font);
    }
    gc_.DrawText(value, x, y);
  }
private:
  wxGraphicsContext &gc_;
  double strokeScale_;
};
}
void RenderPhysicalViewportPreview(wxGCDC &dc, const CommandBuffer &buffer,
    const viewer2d::Viewer2DRenderMapping &mapping, const SymbolDefinitionSnapshot *symbols, double paperToOutput) {
  if (auto *gc = dc.GetGraphicsContext()) {
    PreviewBackend backend(*gc, viewer2d::PaperStrokeScale(mapping.scale, paperToOutput));
    viewer2d::Viewer2DCommandRenderer renderer(mapping, backend, symbols);
    renderer.Render(buffer);
  }
}
