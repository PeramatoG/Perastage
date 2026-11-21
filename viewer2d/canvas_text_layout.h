#pragma once

#include "canvas2d.h"
#include "typography/composition.h"
#include "typography/font_resources.h"

namespace viewer2d {
struct CanvasTextLayout {
  typography::Composition run;
  double baselineOffset = 0;
};

inline CanvasTextLayout ComposeCanvasText(const std::string &text,
    const CanvasTextStyle &style, double scale, const typography::FontCatalog &catalog) {
  const auto size = style.fontSize * scale;
  const auto align = static_cast<int>(style.hAlign);
  const bool bold = style.fontFamily.ends_with("bold");
  CanvasTextLayout layout{typography::Compose(text,
      {size, typography::TextUnit::SceneMetres, bold, 0, align}, catalog, {})};
  const auto ascent = layout.run.ascent, descent = layout.run.descent;
  using Align = CanvasTextStyle::VerticalAlign;
  switch (style.vAlign) {
  case Align::Top: layout.baselineOffset = -ascent; break;
  case Align::Bottom: layout.baselineOffset = descent; break;
  case Align::Middle: layout.baselineOffset = -(ascent - descent) * 0.5; break;
  case Align::Baseline: break;
  }
  if (style.lineHeight > 0 && layout.run.lineHeight > 0) {
    const auto factor = (style.lineHeight + style.extraLineSpacing) * scale / layout.run.lineHeight;
    for (auto &glyph : layout.run.glyphs) glyph.baseline *= factor;
  }
  return layout;
}
}
