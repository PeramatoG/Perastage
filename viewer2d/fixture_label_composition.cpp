#include "fixture_label_composition.h"
#include <algorithm>

namespace viewer2d {
bool HasFieldPresentation(const FixtureLabelOverrideMap &all,
    const std::string &uuid, int view, int field) {
  view = std::clamp(view, 0, 2);
  for (const auto &key : {uuid, std::string(kLabelDefaultsKey)}) {
    auto it = all.find(key);
    if (it != all.end() && it->second.fieldStyles[view][field]) return true;
  }
  return false;
}
FixtureLabelComposition ComposeFixtureLabels(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const Fixture &fixture, int view,
    const LabelBounds &bounds, const typography::FontPair &fonts) {
  FixtureLabelComposition out;
  const std::array<std::string, 3> values{
    fixture.instanceName.empty() ? fixture.uuid : fixture.instanceName,
    "ID: " + std::to_string(fixture.fixtureId), fixture.address};
  for (int f = 0; f < 3; ++f) {
    if (!HasFieldPresentation(all, fixture.uuid, view, f)) continue;
    const auto style = ResolveFieldStyle(cfg, all, fixture.uuid, view, f);
    if (!style.visible || values[f].empty()) continue;
    typography::TextStyle textStyle;
    textStyle.size = style.size; textStyle.bold = style.bold;
    textStyle.alignment = style.alignment;
    const auto composed = typography::Compose(values[f], textStyle, style.bold ? fonts.bold : fonts.regular);
    if (!composed.unsupportedBytes.empty()) {
      out.diagnostics.push_back("Fixture " + fixture.uuid + ": unsupported label characters");
      // Visible, identical diagnostic in preview and PDF; never silently lose text.
      textStyle.size = std::min<double>(style.size, 0.12);
    }
    const auto diagnostic = composed.unsupportedBytes.empty() ? composed :
        typography::Compose("[unsupported text]", textStyle, style.bold ? fonts.bold : fonts.regular);
    double x = (bounds.minX + bounds.maxX) / 2, y = (bounds.minY + bounds.maxY) / 2;
    if (style.anchor == 1) y = bounds.maxY;
    if (style.anchor == 2) y = bounds.minY;
    if (style.anchor == 3) x = bounds.minX;
    if (style.anchor == 4) x = bounds.maxX;
    FixtureLabelComposition::Field field;
    field.first = out.text.size();
    for (const auto &g : diagnostic.glyphs) {
      CanvasTextStyle canvas;
      canvas.fontFamily = style.bold ? "perastage-sans-bold" : "perastage-sans";
      canvas.fontSize = textStyle.size;
      canvas.ascent = diagnostic.ascent; canvas.descent = diagnostic.descent;
      canvas.lineHeight = diagnostic.lineHeight;
      canvas.color = {style.color[0], style.color[1], style.color[2], 1};
      canvas.outlineColor = {style.outlineColor[0], style.outlineColor[1], style.outlineColor[2], 1};
      canvas.outlineWidth = style.outlineEnabled ? style.outlineWidth : 0;
      const double gx = x + style.x + g.x, gy = y + style.y + g.baseline;
      const LabelBounds glyph{gx - canvas.outlineWidth, gy - diagnostic.descent - canvas.outlineWidth,
                              gx + g.advance + canvas.outlineWidth, gy + diagnostic.ascent + canvas.outlineWidth};
      if (!field.count) field.bounds = glyph;
      else {
        field.bounds.minX = std::min(field.bounds.minX,glyph.minX);
        field.bounds.minY = std::min(field.bounds.minY,glyph.minY);
        field.bounds.maxX = std::max(field.bounds.maxX,glyph.maxX);
        field.bounds.maxY = std::max(field.bounds.maxY,glyph.maxY);
      }
      ++field.count;
      out.text.push_back({static_cast<float>(x + style.x + g.x),
                         static_cast<float>(y + style.y + g.baseline), g.text, canvas});
    }
    if (field.count) out.fields[f] = field;
  }
  return out;
}

const FixtureLabelComposition &FixtureLabelCompositionCache::Resolve(
    const ConfigManager &cfg, const FixtureLabelOverrideMap &all,
    const Fixture &fixture, int view, const LabelBounds &bounds,
    const typography::FontPair &fonts) {
  if (resourceRoot_ != fonts.resourceRoot) {
    Clear();
    resourceRoot_ = fonts.resourceRoot;
  }
  Key key{fixture.instanceName, fixture.address, fixture.fixtureId, view, bounds, {}};
  for (int field = 0; field < 3; ++field)
    if (HasFieldPresentation(all, fixture.uuid, view, field))
      key.styles[field] = ResolveFieldStyle(cfg, all, fixture.uuid, view, field);
  auto it = entries_.find(fixture.uuid);
  if (it == entries_.end() || !(it->second.key == key)) {
    auto composition = ComposeFixtureLabels(cfg, all, fixture, view, bounds, fonts);
    it = entries_.insert_or_assign(fixture.uuid,
        Entry{std::move(key), std::move(composition)}).first;
  }
  return it->second.composition;
}
} // namespace viewer2d
