/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "../core/layouts/FixtureLabelPresentationSerialization.h"
#include "fixture_label_presentation.h"
#include "LayoutCollection.h"
namespace viewer2d {
FixtureLabelOverrideMap SnapshotLayoutLabelEditorInheritance(const ConfigManager &cfg,
    const layouts::Layout2DViewRenderOptions &options) {
  auto project = LoadFixtureLabelOverrides(cfg);
  auto &defaults = project[kLabelDefaultsKey];
  for (int v=0; v<3; ++v) {
    defaults.showLabelName[v] = options.showLabelName[v];
    defaults.showLabelId[v] = options.showLabelId[v];
    defaults.showLabelDmx[v] = options.showLabelDmx[v];
    defaults.labelOffsetDistance[v] = options.labelOffsetDistance[v];
    defaults.labelOffsetAngle[v] = options.labelOffsetAngle[v];
  }
  defaults.labelFontSizeName = options.labelFontSizeName;
  defaults.labelFontSizeId = options.labelFontSizeId;
  defaults.labelFontSizeDmx = options.labelFontSizeDmx;
  return project;
}

FixtureLabelOverrideMap OverlayLabelPresentation(const FixtureLabelOverrideMap &project,
    const layouts::FixtureLabelPresentation &p, const layouts::FixtureLabelProfiles &profiles) {
  auto result = project;
  LabelFieldStyles fields;
  if (!p.profileId.empty()) {
    if (!profiles.contains(p.profileId)) throw std::invalid_argument("Missing fixture label style profile: " + p.profileId);
    fields = profiles.at(p.profileId).fields;
  }
  const auto overlay = [](LabelFieldStyles &to, const LabelFieldStyles &from) {
    for (int v=0; v<3; ++v) for (int f=0; f<3; ++f) if (from[v][f]) to[v][f] = from[v][f];
  };
  overlay(fields, p.fields);
  // Apply viewport fields to existing fixture records, then fixture-local fields.
  for (auto &[id, entry] : result) overlay(entry.fieldStyles, fields);
  overlay(result[kLabelDefaultsKey].fieldStyles, fields);
  for (const auto &[id, local] : p.fixtures) {
    overlay(result[id].fieldStyles, fields);
    overlay(result[id].fieldStyles, local);
  }
  return result;
}
std::shared_ptr<const FixtureLabelPresentationContext> BuildLabelPresentationContext(
    const ConfigManager &cfg, const layouts::FixtureLabelPresentation &p,
    const layouts::FixtureLabelProfiles &profiles) {
  if (layouts::LabelPresentationJson(p).empty()) return {};
  auto project = LoadFixtureLabelOverrides(cfg);
  // Freeze resolved project fields before overlaying higher-priority fields.
  for (const auto &[uuid, fixture] : cfg.GetScene().fixtures)
    for (int v=0; v<3; ++v) for (int f=0; f<3; ++f)
      if (HasFieldPresentation(project, uuid, v, f))
        project[uuid].fieldStyles[v][f] = ResolveFieldStyle(cfg, project, uuid, v, f);
  // Snapshot inherited numeric inputs as well. All fields use the canonical
  // composition, without rewriting legacy project settings.
  for (const auto &[uuid, fixture] : cfg.GetScene().fixtures) {
    auto &entry = project[uuid];
    entry.labelFontSizeName = ResolveLabelFontSizeName(cfg, &entry);
    entry.labelFontSizeId = ResolveLabelFontSizeId(cfg, &entry);
    entry.labelFontSizeDmx = ResolveLabelFontSizeDmx(cfg, &entry);
    for (int v=0; v<3; ++v) {
      entry.showLabelName[v] = ResolveShowLabelName(cfg, &entry, v);
      entry.showLabelId[v] = ResolveShowLabelId(cfg, &entry, v);
      entry.showLabelDmx[v] = ResolveShowLabelDmx(cfg, &entry, v);
      entry.labelOffsetDistance[v] = ResolveLabelOffsetDistance(cfg, &entry, v);
      entry.labelOffsetAngle[v] = ResolveLabelOffsetAngle(cfg, &entry, v);
    }
  }
  return std::make_shared<const FixtureLabelPresentationContext>(
      FixtureLabelPresentationContext{OverlayLabelPresentation(project, p, profiles)});
}
} // namespace viewer2d
