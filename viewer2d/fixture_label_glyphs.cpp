#include "fixture_label_composition.h"
#include "fixture_label_name.h"
#include "typography/composition.h"
#include "typography/glyph_outline.h"
#include <algorithm>

namespace viewer2d {
namespace {
size_t ScalarBytes(uint32_t codepoint) {
  return codepoint < 0x80 ? 1 : codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
}
}

FixtureLabelComposition ComposeResolvedFixtureLabels(
    const std::array<LabelFieldStyle, 3> &resolved, const Fixture &fixture,
    const LabelBounds &bounds, const typography::FontPair &fonts) {
  FixtureLabelComposition out;
  const std::array<std::string, 3> values{
    WrapFixtureLabelName(fixture.instanceName.empty() ? fixture.uuid : fixture.instanceName),
    "ID: " + std::to_string(fixture.fixtureId), fixture.address};
  for (int f = 0; f < 3; ++f) {
    const auto &style = resolved[f];
    if (!style.visible || values[f].empty()) continue;
    typography::TextStyle textStyle;
    textStyle.size = style.size; textStyle.bold = style.bold;
    textStyle.alignment = style.alignment;
    const auto composed = typography::Compose(values[f], textStyle, *fonts.catalog, style.fontFamily, style.italic);
    std::string issue;
    if (composed.fontIssue != typography::FontIssue::None)
      issue = typography::DescribeFontIssue(*fonts.catalog, style.fontFamily, style.bold, composed.fontIssue, style.italic);
    if (!composed.unsupportedBytes.empty()) {
      if (!issue.empty()) issue += "\n";
      issue += "Some fixture label characters are unavailable in the approved fonts.";
    }
    if (!issue.empty()) out.diagnostics.push_back(issue);
    double x = (bounds.minX + bounds.maxX) / 2, y = (bounds.minY + bounds.maxY) / 2;
    if (style.anchor == 1) y = bounds.maxY;
    if (style.anchor == 2) y = bounds.minY;
    if (style.anchor == 3) x = bounds.minX;
    if (style.anchor == 4) x = bounds.maxX;
    FixtureLabelComposition::Field field;
    field.first = out.text.size();
    for (const auto &g : composed.glyphs) {
      CanvasTextStyle canvas;
      const auto face = fonts.catalog->FindResource(g.resourceId);
      if (face && face->family == typography::kDefaultFamily && !face->italic &&
          !g.clusterContinuation && g.text.size() == ScalarBytes(g.codepoint) &&
          face->metrics.GlyphFor(g.codepoint) == g.glyphId)
        canvas.fontFamily = face->bold ? "perastage-sans-bold" : "perastage-sans";
      canvas.fontSize = textStyle.size;
      canvas.ascent = composed.ascent; canvas.descent = composed.descent;
      canvas.lineHeight = composed.lineHeight;
      canvas.color = {style.color[0], style.color[1], style.color[2], 1};
      canvas.outlineColor = {style.outlineColor[0], style.outlineColor[1], style.outlineColor[2], 1};
      canvas.outlineWidth = style.outlineEnabled ? style.outlineWidth : 0;
      canvas.fontResource = g.resourceId;
      canvas.codepoint = g.codepoint;
      canvas.glyphId = g.glyphId;
      canvas.clusterContinuation = g.clusterContinuation;
      if (field.count == 0) canvas.fontDiagnostic = issue;
      canvas.glyphOutline = typography::ResolveGlyphOutline(*fonts.catalog, g.resourceId, g.glyphId);
      if (!canvas.glyphOutline || canvas.glyphOutline->substituted) {
        const std::string diagnostic = "A fixture label glyph contour is unavailable in the approved face.";
        if (std::find(out.diagnostics.begin(), out.diagnostics.end(), diagnostic) == out.diagnostics.end())
          out.diagnostics.push_back(diagnostic);
        canvas.fontFamily.clear();
      }
      ++field.count;
      out.text.push_back({static_cast<float>(x + style.x + g.x),
                         static_cast<float>(y + style.y + g.baseline), g.text, canvas});
    }
    if (field.count) {
      const auto selected = fonts.catalog->FindResource(composed.resourceId);
      if (selected && selected->source == typography::FontSource::InstalledSystem) {
        auto &first = out.text[field.first];
        first.style.source = std::make_shared<CanvasTextSource>(CanvasTextSource{
            values[f], style.fontFamily, static_cast<float>(x + style.x - first.x),
            static_cast<float>(y + style.y - first.y), field.count, style.bold, style.italic, style.alignment});
      }
      const auto stroke = style.outlineEnabled ? style.outlineWidth : 0;
      const auto &box = composed.bounds;
      field.bounds = {x + style.x + box.left - stroke, y + style.y + box.bottom - stroke,
                      x + style.x + box.right + stroke, y + style.y + box.top + stroke};
      out.fields[f] = field;
    }
  }
  return out;
}

} // namespace viewer2d
