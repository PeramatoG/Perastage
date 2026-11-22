#pragma once

#include "canvas2d.h"
#include <algorithm>
#include "typography/composition.h"
#include "typography/font_resources.h"

namespace viewer2d {
struct CanvasFontRequest {
  std::string_view family;
  bool bold = false;
  bool italic = false;
};

inline CanvasFontRequest CanvasFont(std::string_view name) {
  if (name == "sans" || name == "perastage-sans") return {};
  if (name == "sans-bold" || name == "perastage-sans-bold") return {{}, true};
  for (const auto variant : {std::string_view(" Bold Italic"), std::string_view(" Italic"),
                             std::string_view(" Bold"), std::string_view(" Regular")}) {
    if (name.size() > variant.size() && name.ends_with(variant))
      return {name.substr(0, name.size() - variant.size()),
              variant.starts_with(" Bold"), variant.ends_with("Italic")};
  }
  return {name};
}

struct CanvasTextLayout {
  typography::Composition run;
  double baselineOffset = 0;
};

inline CanvasTextLayout ComposeCanvasText(const std::string &text,
    const CanvasTextStyle &style, double scale, const typography::FontCatalog &catalog) {
  const auto size = style.fontSize * scale;
  const auto align = static_cast<int>(style.hAlign);
  const auto request = CanvasFont(style.fontFamily);
  CanvasTextLayout layout{typography::Compose(text,
      {size, typography::TextUnit::SceneMetres, request.bold, 0, align}, catalog, request.family, request.italic)};
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
    double bottom = layout.run.bounds.bottom;
    if (!layout.run.lines.empty()) bottom = layout.run.lines.front().baseline - descent;
    for (auto &line : layout.run.lines) {
      const auto offset = line.baseline * (factor - 1);
      line.baseline += offset;
      bottom = std::min(bottom, line.baseline - descent);
      for (size_t i = line.first; i < line.first + line.count; ++i) {
        auto &glyph = layout.run.glyphs[i];
        glyph.baseline += offset;
        bottom = std::min(bottom, glyph.baseline + glyph.ink.bottom);
      }
    }
    layout.run.bounds.bottom = bottom;
    layout.run.lineHeight *= factor;
  }
  return layout;
}
}
