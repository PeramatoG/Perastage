#include "fixture_label_composition.h"
#include "fixture_label_name.h"
#include <algorithm>
#include "typography/glyph_outline.h"
#include "typography/composition.h"

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
std::array<LabelFieldStyle, 3> ResolveEditableFieldStyles(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const std::string &uuid, int view,
    const typography::FontPair &fonts, const Fixture *source) {
  // Adapt legacy stack baselines once, identically for preview and property edits.
  FixtureLabelOverrideMap legacy;
  for (const auto &key : {uuid, std::string(kLabelDefaultsKey)}) {
    if (const auto it = all.find(key); it != all.end()) {
      auto entry = it->second;
      entry.fieldStyles = {};
      legacy.emplace(key, std::move(entry));
    }
  }
  std::array<LabelFieldStyle, 3> result;
  double groupHeight = 0;
  std::array<size_t, 3> lineCounts{1,1,1};
  if (!source) {
    const auto it = cfg.GetScene().fixtures.find(uuid);
    if (it != cfg.GetScene().fixtures.end()) source = &it->second;
  }
  if (source) {
    const auto name = WrapFixtureLabelName(source->instanceName.empty() ? uuid : source->instanceName);
    lineCounts[0] += std::count(name.begin(), name.end(), '\n');
  }
  for (int f = 0; f < 3; ++f) {
    result[f] = ResolveFieldStyle(cfg, legacy, uuid, view, f);
    const auto &s = result[f];
    const auto &font = s.bold ? fonts.bold : fonts.regular;
    if (s.visible) groupHeight += (font.ascent + std::abs(font.descent)) * s.size / font.unitsPerEm +
        (lineCounts[f] - 1) * (font.ascent + std::abs(font.descent) + font.lineGap) * s.size / font.unitsPerEm;
  }
  double top = groupHeight / 2;
  for (int f = 0; f < 3; ++f) {
    auto &s = result[f];
    const auto &font = s.bold ? fonts.bold : fonts.regular;
    const double ascent = font.ascent * s.size / font.unitsPerEm;
    const double height = (font.ascent + std::abs(font.descent)) * s.size / font.unitsPerEm;
    s.y += f * s.size + top - ascent;
    if (s.visible) top -= height + (lineCounts[f] - 1) *
        (font.ascent + std::abs(font.descent) + font.lineGap) * s.size / font.unitsPerEm;
    if (HasFieldPresentation(all, uuid, view, f)) s = ResolveFieldStyle(cfg, all, uuid, view, f);
  }
  return result;
}

FixtureLabelComposition ComposeFixtureLabels(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const Fixture &fixture, int view,
    const LabelBounds &bounds, const typography::FontPair &fonts) {
  FixtureLabelComposition out;
  const std::array<std::string, 3> values{
    WrapFixtureLabelName(fixture.instanceName.empty() ? fixture.uuid : fixture.instanceName),
    "ID: " + std::to_string(fixture.fixtureId), fixture.address};
  const auto resolved = ResolveEditableFieldStyles(cfg, all, fixture.uuid, view, fonts, &fixture);
  for (int f = 0; f < 3; ++f) {
    const auto &style = resolved[f];
    if (!style.visible || values[f].empty()) continue;
    typography::TextStyle textStyle;
    textStyle.size = style.size; textStyle.bold = style.bold;
    textStyle.alignment = style.alignment;
    const auto composed = typography::Compose(values[f], textStyle, *fonts.catalog, style.fontFamily);
    std::string issue;
    if (composed.fontIssue != typography::FontIssue::None)
      issue = typography::DescribeFontIssue(*fonts.catalog, style.fontFamily, style.bold, composed.fontIssue);
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
      const auto *face = fonts.catalog->FindResource(g.resourceId);
      if (face && face->family == typography::kDefaultFamily && !face->italic)
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
      if (field.count == 0) canvas.fontDiagnostic = issue;
      canvas.glyphOutline = typography::ResolveGlyphOutline(*fonts.catalog, g.resourceId, g.glyphId);
      const double gx = x + style.x + g.x, gy = y + style.y + g.baseline;
      const LabelBounds glyph{gx - canvas.outlineWidth, gy - composed.descent - canvas.outlineWidth,
                              gx + g.advance + canvas.outlineWidth, gy + composed.ascent + canvas.outlineWidth};
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
  if (catalog_ != fonts.catalog) {
    Clear();
    catalog_ = fonts.catalog;
  }
  Key key{fixture.instanceName, fixture.address, fixture.fixtureId, view, bounds, {}};
  const auto resolved = ResolveEditableFieldStyles(cfg, all, fixture.uuid, view, fonts, &fixture);
  for (int field = 0; field < 3; ++field) key.styles[field] = resolved[field];
  auto it = entries_.find(fixture.uuid);
  if (it == entries_.end() || !(it->second.key == key)) {
    if (entries_.size() >= 20000 && it == entries_.end()) entries_.clear();
    auto composition = ComposeFixtureLabels(cfg, all, fixture, view, bounds, fonts);
    it = entries_.insert_or_assign(fixture.uuid,
        Entry{std::move(key), std::move(composition)}).first;
  }
  return it->second.composition;
}
} // namespace viewer2d
