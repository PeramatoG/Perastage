#pragma once
#include "canvas2d.h"
#include "typography/text_outline.h"
#include <nanovg.h>

namespace viewer2d {
// Draw within the caller's NanoVG frame, using precomposed baselines and units.
inline void DrawCanvasText(NVGcontext *vg, const TextCommand &text,
                           float x, float y, double pixelsPerUnit,
                           typography::TextInkPass pass = typography::TextInkPass::Both) {
  const auto &style = text.style;
  nvgFontFace(vg, style.fontFamily.c_str());
  nvgFontSize(vg, (style.ascent + style.descent) * pixelsPerUnit);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
  if (pass != typography::TextInkPass::Fill && style.outlineWidth > 0) {
    nvgFillColor(vg, nvgRGBAf(style.outlineColor.r, style.outlineColor.g,
                             style.outlineColor.b, style.outlineColor.a));
    for (const auto &offset : typography::OutlineOffsets(style.outlineWidth * pixelsPerUnit))
      nvgText(vg, x + offset[0], y + offset[1], text.text.c_str(), nullptr);
  }
  if (pass != typography::TextInkPass::Outline) {
    nvgFillColor(vg, nvgRGBAf(style.color.r,style.color.g,style.color.b,style.color.a));
    nvgText(vg,x,y,text.text.c_str(),nullptr);
  }
}
} // namespace viewer2d
