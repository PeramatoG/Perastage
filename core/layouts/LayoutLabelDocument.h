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
#pragma once
#include "FixtureLabelPresentationSerialization.h"
#include "LayoutTemplateSerializer.h"
namespace layouts {
inline void ValidateLabelReferences(const LayoutCollection &c) {
  LabelProfilesJson(c.labelProfiles);
  for (const auto &page : c.Items()) for (const auto &view : page.view2dViews) {
    const auto &p = view.labelPresentation;
    if (!p.profileId.empty() && !c.labelProfiles.contains(p.profileId))
      throw std::invalid_argument("Missing fixture label style profile: " + p.profileId);
    auto j = LabelPresentationJson(p);
    if (!j.empty()) ReadLabelPresentation(j);
  }
}
inline nlohmann::json LayoutDocumentJson(const LayoutCollection &c) {
  ValidateLabelReferences(c);
  auto j = ToTemplateDocument(c.Items());
  if (!c.labelProfiles.empty()) j["fixtureLabelProfiles"] = LabelProfilesJson(c.labelProfiles);
  return j;
}
inline void ReadLayoutLabelProfiles(const nlohmann::json &j, LayoutCollection &c) {
  c.labelProfiles = j.is_object() && j.contains("fixtureLabelProfiles")
      ? ReadLabelProfiles(j.at("fixtureLabelProfiles")) : FixtureLabelProfiles{};
  ValidateLabelReferences(c);
}
// Reuse equal definitions, otherwise choose the first deterministic free suffix.
inline void ImportLabelDependencies(LayoutDefinition &page, FixtureLabelProfiles &dest) {
  std::map<std::string, std::string> remap;
  for (const auto &[oldId, profile] : page.portableLabelProfiles) {
    auto id = oldId;
    int suffix = 2;
    while (dest.contains(id) && !(dest.at(id).name == profile.name && dest.at(id).fields == profile.fields))
      id = oldId + "-" + std::to_string(suffix++);
    auto copy = profile; copy.id = id;
    dest.emplace(id, std::move(copy));
    remap[oldId] = id;
  }
  for (auto &v : page.view2dViews)
    if (const auto it = remap.find(v.labelPresentation.profileId); it != remap.end()) v.labelPresentation.profileId = it->second;
  page.portableLabelProfiles.clear();
}
} // namespace layouts
