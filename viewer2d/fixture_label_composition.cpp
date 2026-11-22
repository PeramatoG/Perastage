#include "fixture_label_composition.h"
#include "fixture_label_name.h"
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
std::array<LabelFieldStyle, 3> ResolveEditableFieldStyles(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const std::string &uuid, int view,
    const typography::FontPair &fonts, const Fixture *source) {
  // Legacy fields share a centered stack. Explicit field positions are authored
  // baselines and remain fixed when a font changes, including sibling fields.
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
  std::array<typography::FontLineMetrics, 3> metrics;
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
    const auto resolved = fonts.catalog->Resolve(s.fontFamily, s.bold);
    if (resolved.face) metrics[f] = typography::MeasureFontLine(resolved.face->metrics, s.size);
    if (s.visible)
      groupHeight += metrics[f].ascent + metrics[f].descent + (lineCounts[f] - 1) * metrics[f].lineHeight;
  }
  double top = groupHeight / 2;
  for (int f = 0; f < 3; ++f) {
    auto &s = result[f];
    const auto [ascent, descent, lineHeight] = metrics[f];
    s.y += f * s.size + top - ascent;
    if (s.visible) top -= ascent + descent + (lineCounts[f] - 1) * lineHeight;
    if (HasFieldPresentation(all, uuid, view, f)) s = ResolveFieldStyle(cfg, all, uuid, view, f);
  }
  return result;
}

FixtureLabelComposition ComposeFixtureLabels(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const Fixture &fixture, int view,
    const LabelBounds &bounds, const typography::FontPair &fonts) {
  return ComposeResolvedFixtureLabels(
      ResolveEditableFieldStyles(cfg, all, fixture.uuid, view, fonts, &fixture),
      fixture, bounds, fonts);
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
    auto composition = ComposeResolvedFixtureLabels(resolved, fixture, bounds, fonts);
    it = entries_.insert_or_assign(fixture.uuid,
        Entry{std::move(key), std::move(composition)}).first;
  }
  return it->second.composition;
}
} // namespace viewer2d
