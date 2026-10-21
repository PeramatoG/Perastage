#pragma once
#include "canvas2d.h"
#include "typography/text_outline.h"
#include "typography/glyph_outline.h"
#include <nanovg.h>

namespace viewer2d {
// Both inks use identical unhinted contours; atlas rounding must not move the fill.
inline void DrawCanvasText(NVGcontext *vg, const TextCommand &text,
                           float x, float y, double pixelsPerUnit,
                           typography::TextInkPass pass = typography::TextInkPass::Both) {
  const auto &style = text.style;
  if (!style.glyphOutline) return;
  const double em = style.fontSize * pixelsPerUnit;
  nvgSave(vg);
  nvgBeginPath(vg);
  for (const auto &p : style.glyphOutline->path) {
    using Kind = typography::GlyphPathElement::Kind;
    switch (p.kind) {
    case Kind::Move: nvgMoveTo(vg, x + p.x * em, y - p.y * em); break;
    case Kind::Line: nvgLineTo(vg, x + p.x * em, y - p.y * em); break;
    case Kind::Quadratic:
      nvgQuadTo(vg, x + p.cx * em, y - p.cy * em, x + p.x * em, y - p.y * em);
      break;
    case Kind::Close:
      nvgClosePath(vg);
      nvgPathWinding(vg, p.hole ? NVG_HOLE : NVG_SOLID);
      break;
    }
  }
  if (pass != typography::TextInkPass::Fill && style.outlineWidth > 0) {
    nvgStrokeColor(vg, nvgRGBAf(style.outlineColor.r, style.outlineColor.g,
                               style.outlineColor.b, style.outlineColor.a));
    nvgStrokeWidth(vg, 2 * style.outlineWidth * pixelsPerUnit);
    nvgLineJoin(vg, NVG_ROUND);
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);
  }
  if (pass != typography::TextInkPass::Outline) {
    nvgFillColor(vg, nvgRGBAf(style.color.r, style.color.g, style.color.b, style.color.a));
    nvgFill(vg);
  }
  nvgRestore(vg);
}
} // namespace viewer2d
