#pragma once
#include "canvas2d.h"
#include "typography/text_outline.h"
#include "typography/glyph_outline.h"
#include "typography/font_resources.h"
#include <nanovg.h>
#include <array>
#include <cstdlib>
#include <cstring>

namespace viewer2d {
// NanoVG distributions can map fontSize to em or to ascent/descent height.
// Measure the loaded face once per frame, then use the exact known TTF ratio.
inline double ResolveRasterFontEmScale(NVGcontext *vg, const char *family,
                                      const typography::TtfFontMetrics &font, double devicePixelRatio = 1) {
  const int id = nvgFindFont(vg, family);
  if (id < 0 || font.unitsPerEm <= 0 || !std::isfinite(devicePixelRatio) || devicePixelRatio <= 0) return 1;
  const double probeSize = 1024 / devicePixelRatio;
  nvgSave(vg);
  nvgFontFaceId(vg, id); nvgFontSize(vg, probeSize);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
  nvgTextLetterSpacing(vg, 0); nvgFontBlur(vg, 0);
  const double measured = nvgTextBounds(vg, 0, 0, "M", nullptr, nullptr);
  nvgRestore(vg);
  if (!std::isfinite(measured) || measured <= 0) return 1;
  const double ratio = font.glyphAdvances[font.GlyphFor('M')] * probeSize / font.unitsPerEm / measured;
  if (std::abs(ratio - 1) < .002) return 1;
  const double heightRatio = double(font.ascent + std::abs(font.descent)) / font.unitsPerEm;
  if (std::abs(ratio - heightRatio) < .002) return heightRatio;
  return ratio;
}

// Only the historical default uses NanoVG's font atlas. Give each context
// copies of the verified bytes, which fontstash owns until context destruction.
inline std::array<double, 2> PrepareDefaultLabelAtlas(NVGcontext *vg,
    const typography::FontPair &fonts, double pixelRatio = 1) {
  std::array<double, 2> result{1, 1};
  for (const bool bold : {false, true}) {
    const auto &font = bold ? fonts.bold : fonts.regular;
    const char *key = bold ? "perastage-sans-bold" : "perastage-sans";
    if (!font.valid) continue;
    const auto expected = fonts.catalog->Resolve({}, bold);
    if (!expected.face || expected.face->bold != bold) continue;
    if (nvgFindFont(vg, key) < 0) {
      auto *bytes = static_cast<unsigned char *>(std::malloc(font.data.size()));
      if (!bytes) continue;
      std::memcpy(bytes, font.data.data(), font.data.size());
      if (nvgCreateFontMem(vg, key, bytes, static_cast<int>(font.data.size()), 1) < 0)
        continue;
    }
    result[bold] = ResolveRasterFontEmScale(vg, key, font, pixelRatio);
  }
  return result;
}

// Composition owns glyph positions. Screen ink uses the established rasterizer
// for both halo and fill, keeping their glyph shape and pixel rounding identical.
inline void DrawCanvasText(NVGcontext *vg, const TextCommand &text,
                           float x, float y, double scenePixelsPerUnit,
                           typography::TextInkPass pass = typography::TextInkPass::Both,
                           double devicePixelRatio = 1, double rasterEmScale = 1) {
  const auto &style = text.style;
  if (!style.glyphOutline) return;
  const double em = style.fontSize * scenePixelsPerUnit;
  nvgSave(vg);
  const int font = style.IsComposedGlyph() && (style.glyphId == 0 || style.codepoint == '\t')
      ? -1 : nvgFindFont(vg, style.fontFamily.c_str());
  nvgTextLetterSpacing(vg, 0);
  nvgFontBlur(vg, 0);
  const auto fill = [&] {
    nvgFillColor(vg, nvgRGBAf(style.color.r, style.color.g, style.color.b, style.color.a));
    if (font >= 0) {
      nvgFontFaceId(vg, font);
      nvgFontSize(vg, em * rasterEmScale);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
      nvgText(vg, x, y, text.text.c_str(), nullptr);
    } else nvgFill(vg); // Non-font synthetic contours retain their vector fill.
  };
  if (font >= 0) {
    nvgFontFaceId(vg, font);
    nvgFontSize(vg, em * rasterEmScale);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    if (pass != typography::TextInkPass::Fill && style.outlineWidth > 0) {
      const double radius = typography::ScreenOutlineStrokeWidth(style.outlineWidth, devicePixelRatio) / 2;
      nvgFillColor(vg, nvgRGBAf(style.outlineColor.r, style.outlineColor.g,
                               style.outlineColor.b, style.outlineColor.a));
      for (const auto &offset : typography::OutlineOffsets(radius))
        nvgText(vg, x + offset[0], y + offset[1], text.text.c_str(), nullptr);
    }
    if (pass != typography::TextInkPass::Outline) fill();
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
    case Kind::Cubic:
      nvgBezierTo(vg, x + p.cx * em, y - p.cy * em, x + p.dx * em, y - p.dy * em,
                  x + p.x * em, y - p.y * em);
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
