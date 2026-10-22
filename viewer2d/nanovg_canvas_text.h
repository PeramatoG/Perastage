#pragma once
#include "canvas2d.h"
#include "typography/text_outline.h"
#include "typography/glyph_outline.h"
#include <nanovg.h>

namespace viewer2d {
// Composition owns glyph positions. Screen fill uses the legacy font rasterizer:
// polygon AA fringes overfill subpixel stems when zoomed out. Halos stay vector.
inline void DrawCanvasText(NVGcontext *vg, const TextCommand &text,
                           float x, float y, double scenePixelsPerUnit,
                           typography::TextInkPass pass = typography::TextInkPass::Both,
                           double devicePixelRatio = 1) {
  const auto &style = text.style;
  if (!style.glyphOutline) return;
  const double em = style.fontSize * scenePixelsPerUnit;
  nvgSave(vg);
  const int font = nvgFindFont(vg, style.fontFamily.c_str());
  const auto fill = [&] {
    nvgFillColor(vg, nvgRGBAf(style.color.r, style.color.g, style.color.b, style.color.a));
    if (font >= 0) {
      nvgFontFaceId(vg, font);
      nvgFontSize(vg, em);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
      nvgText(vg, x, y, text.text.c_str(), nullptr);
    } else nvgFill(vg); // Non-font synthetic contours retain their vector fill.
  };
  if (pass == typography::TextInkPass::Fill && font >= 0) {
    fill();
    nvgRestore(vg);
    return;
  }
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
    nvgStrokeWidth(vg, typography::ScreenOutlineStrokeWidth(style.outlineWidth, devicePixelRatio));
    nvgLineJoin(vg, NVG_ROUND);
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);
  }
  if (pass != typography::TextInkPass::Outline) {
    fill();
  }
  nvgRestore(vg);
}
} // namespace viewer2d
