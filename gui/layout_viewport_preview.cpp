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
#include "canvas_text_layout.h"
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
  PreviewBackend(wxGraphicsContext &gc, double strokeScale, double textScale, bool textOnly)
      : gc_(gc), strokeScale_(strokeScale), textScale_(textScale), textOnly_(textOnly),
        catalog_(typography::GetFontCatalog(ProjectUtils::GetResourceRoot())) {}

  void FlushText() {
    if (text_.empty()) return;
    for (const auto &ink : text_) {
      if (ink.width <= 0) continue;
      gc_.SetPen(gc_.CreatePen(wxGraphicsPenInfo(ink.outline).Width(2 * ink.width)
                                  .Cap(wxCAP_ROUND).Join(wxJOIN_ROUND)));
      gc_.StrokePath(ink.path);
    }
    for (const auto &ink : text_) {
      gc_.SetBrush(wxBrush(ink.fill));
      gc_.FillPath(ink.path, wxWINDING_RULE);
    }
    text_.clear();
  }

  void Stroke(const CanvasStroke &s, double width, const CanvasFill *f = nullptr) {
    if (width > 0)
      gc_.SetPen(gc_.CreatePen(wxGraphicsPenInfo(Color(s.color)).Width(width * strokeScale_)
          .Cap(wxCAP_ROUND).Join(wxJOIN_ROUND)));
    else gc_.SetPen(*wxTRANSPARENT_PEN);
    gc_.SetBrush(f ? wxBrush(Color(f->color)) : *wxTRANSPARENT_BRUSH);
  }
  void DrawLine(const viewer2d::Viewer2DRenderPoint &a, const viewer2d::Viewer2DRenderPoint &b,
      const CanvasStroke &s, double width) override {
    FlushText();
    if (textOnly_) return;
    Stroke(s, width); gc_.StrokeLine(a.x, a.y, b.x, b.y);
  }
  void Path(const std::vector<viewer2d::Viewer2DRenderPoint> &p,
      const CanvasStroke &s, double width, const CanvasFill *f, bool closed) {
    FlushText();
    if (textOnly_ || p.empty()) return;
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
    FlushText();
    if (textOnly_) return;
    Stroke(s, width, f);
    auto path = gc_.CreatePath(); path.AddEllipse(p.x-radius, p.y-radius, 2*radius, 2*radius);
    if (f) gc_.FillPath(path);
    if (width > 0) gc_.StrokePath(path);
  }
  void DrawText(const viewer2d::Viewer2DRenderText &t) override {
    if (t.fontSizePx <= 0) return;
    if (source_ != t.source || !t.style.IsComposedGlyph()) FlushText();
    source_ = t.source;
    const auto add = [&](const typography::GlyphOutline &outline, double x, double y) {
      if (outline.path.empty()) return;
      text_.push_back({gui::BuildDocumentGlyphPath(gc_, outline, x, y, t.fontSizePx),
                       Color(t.style.color), Color(t.style.outlineColor), t.outlineWidthPx});
    };
    if (t.style.IsComposedGlyph()) {
      if (!catalog_->FindResource(t.style.fontResource)) return;
      const auto outline = t.style.glyphOutline ? t.style.glyphOutline
          : typography::ResolveGlyphOutline(*catalog_, t.style.fontResource, t.style.glyphId);
      if (outline) add(*outline, t.anchor.x, t.anchor.y);
    } else {
      const auto layout = viewer2d::ComposeCanvasText(t.text, t.style, textScale_, *catalog_);
      if (layout.run.fontIssue != typography::FontIssue::None) {
        const auto request = viewer2d::CanvasFont(t.style.fontFamily);
        gui::ReportDocumentTypographyDiagnostic(typography::DescribeFontIssue(
            *catalog_, request.family, request.bold, layout.run.fontIssue, request.italic));
      }
      if (!layout.run.unsupportedBytes.empty())
        gui::ReportDocumentTypographyDiagnostic("Some text characters are unavailable in the approved fonts.");
      for (const auto &glyph : layout.run.glyphs) {
        const auto outline = typography::ResolveGlyphOutline(*catalog_, glyph.resourceId, glyph.glyphId);
        if (outline) add(*outline, t.anchor.x + glyph.x,
                         t.anchor.y - layout.baselineOffset - glyph.baseline);
      }
      FlushText();
    }
  }
private:
  struct TextInk {
    wxGraphicsPath path;
    wxColour fill, outline;
    double width;
  };
  wxGraphicsContext &gc_;
  double strokeScale_, textScale_;
  bool textOnly_;
  std::shared_ptr<const typography::FontCatalog> catalog_;
  std::vector<TextInk> text_;
  std::string_view source_;

};
}
void RenderPhysicalViewportPreview(wxGCDC &dc, const CommandBuffer &buffer,
    const viewer2d::Viewer2DRenderMapping &mapping, const SymbolDefinitionSnapshot *symbols, double paperToOutput, bool textOnly) {
  if (auto *gc = dc.GetGraphicsContext()) {
    PreviewBackend backend(*gc, viewer2d::PaperStrokeScale(mapping.scale, paperToOutput), mapping.scale, textOnly);
    viewer2d::Viewer2DCommandRenderer renderer(mapping, backend, symbols);
    renderer.Render(buffer);
    backend.FlushText();
  }
}
