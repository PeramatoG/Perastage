#include "fixture_label_composition.h"
#include "fixture_label_name.h"
#include "performance_metrics.h"
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
namespace {
struct StyleInputs {
  std::array<std::optional<LabelFieldStyle>, 3> authored;
  std::array<LabelFieldStyle, 3> stack;
};

bool IsFullyAuthored(const StyleInputs &inputs) {
  return std::all_of(inputs.authored.begin(), inputs.authored.end(),
                     [](const auto &style) { return style.has_value(); });
}

StyleInputs ResolveStyleInputs(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const std::string &uuid, int view) {
  StyleInputs inputs;
  for (int field = 0; field < 3; ++field)
    if (HasFieldPresentation(all, uuid, view, field))
      inputs.authored[field] = ResolveFieldStyle(cfg, all, uuid, view, field);
  if (IsFullyAuthored(inputs))
    return inputs;

  FixtureLabelOverrideMap legacy;
  for (const auto &key : {uuid, std::string(kLabelDefaultsKey)}) {
    if (const auto it = all.find(key); it != all.end()) {
      auto entry = it->second;
      entry.fieldStyles = {};
      legacy.emplace(key, std::move(entry));
    }
  }
  for (int field = 0; field < 3; ++field)
    inputs.stack[field] = ResolveFieldStyle(cfg, legacy, uuid, view, field);
  return inputs;
}

std::array<LabelFieldStyle, 3> PositionFieldStyles(
    const StyleInputs &inputs, const std::string &uuid,
    const typography::FontPair &fonts, const Fixture *source) {
  // Legacy fields share a centered stack. Explicit baselines are authored.
  if (IsFullyAuthored(inputs))
    return {*inputs.authored[0], *inputs.authored[1], *inputs.authored[2]};

  auto result = inputs.stack;
  double groupHeight = 0;
  std::array<size_t, 3> lineCounts{1,1,1};
  std::array<typography::FontLineMetrics, 3> metrics;
  if (source) {
    const auto name = WrapFixtureLabelName(source->instanceName.empty() ? uuid : source->instanceName);
    lineCounts[0] += std::count(name.begin(), name.end(), '\n');
  }
  for (int f = 0; f < 3; ++f) {
    const auto &s = result[f];
    const auto resolved = fonts.catalog->Resolve(s.fontFamily, s.bold, s.italic);
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
    if (inputs.authored[f])
      s = *inputs.authored[f];
  }
  return result;
}

} // namespace

std::array<LabelFieldStyle, 3> ResolveEditableFieldStyles(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const std::string &uuid, int view,
    const typography::FontPair &fonts, const Fixture *source) {
  if (!source) {
    const auto it = cfg.GetScene().fixtures.find(uuid);
    if (it != cfg.GetScene().fixtures.end())
      source = &it->second;
  }
  return PositionFieldStyles(ResolveStyleInputs(cfg, all, uuid, view), uuid, fonts, source);
}

FixtureLabelComposition ComposeFixtureLabels(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const Fixture &fixture, int view,
    const LabelBounds &bounds, const typography::FontPair &fonts) {
  return ComposeResolvedFixtureLabels(
      ResolveEditableFieldStyles(cfg, all, fixture.uuid, view, fonts, &fixture),
      fixture, bounds, fonts);
}

void FixtureLabelCompositionCache::PruneRemovedFixtures(const MvrScene &scene) {
  const auto removed = std::erase_if(entries_, [&](const auto &entry) {
    return !scene.fixtures.contains(entry.first);
  });
  if (performance::viewer2DFrameActive)
    performance::Count(performance::Work::View2DLabelRemoved, removed);
}

const FixtureLabelComposition &FixtureLabelCompositionCache::Resolve(
    const ConfigManager &cfg, const FixtureLabelOverrideMap &all,
    const Fixture &fixture, int view, const LabelBounds &bounds,
    const typography::FontPair &fonts) {
  const bool profile = performance::viewer2DFrameActive;
  if (catalog_ != fonts.catalog) {
    if (profile)
      performance::Count(performance::Work::View2DLabelFont, entries_.size());
    Clear();
    catalog_ = fonts.catalog;
  }
  auto styles = ResolveStyleInputs(cfg, all, fixture.uuid, view);
  const auto it = entries_.find(fixture.uuid);
  if (it != entries_.end()) {
    const auto &key = it->second.key;
    const bool sameText = key.name == fixture.instanceName &&
        key.address == fixture.address && key.id == fixture.fixtureId;
    const bool sameOrientation = key.view == view;
    const bool sameAnchor = key.bounds == bounds;
    const bool sameStyle = key.authoredStyles == styles.authored &&
                           key.stackStyles == styles.stack;
    if (sameText && sameOrientation && sameAnchor && sameStyle)
      return it->second.composition;
    if (profile) {
      performance::Count(performance::Work::View2DLabelContent, !sameText);
      performance::Count(performance::Work::View2DLabelOrientation, !sameOrientation);
      performance::Count(performance::Work::View2DLabelAnchor, !sameAnchor);
      performance::Count(performance::Work::View2DLabelStyle, !sameStyle);
    }
  } else if (profile) {
    performance::Count(performance::Work::View2DLabelNew);
  }
  performance::Scope metric(performance::Work::View2DLabelCompose, profile);
  if (profile)
    performance::Count(performance::Work::View2DCompositions);
  auto composition = ComposeResolvedFixtureLabels(
      PositionFieldStyles(styles, fixture.uuid, fonts, &fixture),
      fixture, bounds, fonts);
  Key key{fixture.instanceName, fixture.address, fixture.fixtureId, view, bounds,
          std::move(styles.authored), std::move(styles.stack)};
  return entries_.insert_or_assign(fixture.uuid,
      Entry{std::move(key), std::move(composition)}).first->second.composition;
}
} // namespace viewer2d
