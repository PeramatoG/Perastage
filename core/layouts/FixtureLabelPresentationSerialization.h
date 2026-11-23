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
#include "FixtureLabelPresentation.h"
#include "../../viewer2d/fixture_label_style_serialization.h"
namespace layouts {
inline nlohmann::json LabelFieldsJson(const viewer2d::LabelFieldStyles &fields) {
  nlohmann::json j = nlohmann::json::object();
  viewer2d::WriteFieldStyles(j, fields);
  return j;
}
inline viewer2d::LabelFieldStyles ReadLabelFields(const nlohmann::json &j) {
  viewer2d::LabelFieldStyles fields;
  if (j.empty()) return fields;
  const auto &p = j.at("presentation");
  if ((p.at("version") != 2 && p.at("version") != 3 && p.at("version") != 4) || !p.at("fields").is_array() || p.at("fields").size() != 3)
    throw std::invalid_argument("Invalid Layout label field presentation.");
  viewer2d::ReadFieldStyles(j, fields);
  for (int v=0; v<3; ++v) {
    if (!p.at("fields")[v].is_array() || p.at("fields")[v].size()!=3)
      throw std::invalid_argument("Invalid Layout label orientation fields.");
    for (int f=0; f<3; ++f)
      if (!p.at("fields")[v][f].is_null() && !fields[v][f])
        throw std::invalid_argument("Invalid Layout label field values.");
  }
  return fields;
}
inline nlohmann::json LabelPresentationJson(const FixtureLabelPresentation &p) {
  auto j = LabelFieldsJson(p.fields);
  if (!p.profileId.empty()) j["profileId"] = p.profileId;
  if (!p.fixtures.empty()) {
    j["fixtures"] = nlohmann::json::object();
    for (const auto &[id, fields] : p.fixtures) j["fixtures"][id] = LabelFieldsJson(fields);
  }
  if (!j.empty()) { j["version"] = 1; j["metrics"] = "sceneMetres"; }
  return j;
}
inline FixtureLabelPresentation ReadLabelPresentation(const nlohmann::json &j) {
  FixtureLabelPresentation p;
  if (j.at("version") != 1 || j.at("metrics") != "sceneMetres")
    throw std::invalid_argument("Unsupported Layout label presentation or metrics.");
  p.profileId = j.value("profileId", std::string{});
  if (j.contains("presentation")) p.fields = ReadLabelFields(j);
  if (j.contains("fixtures")) {
    if (!j.at("fixtures").is_object()) throw std::invalid_argument("Invalid local label fixtures.");
    for (const auto &[id, fields] : j.at("fixtures").items()) p.fixtures[id] = ReadLabelFields(fields);
  }
  return p;
}
inline nlohmann::json LabelProfilesJson(const FixtureLabelProfiles &profiles) {
  auto j = nlohmann::json::array();
  for (const auto &[id, p] : profiles) {
    if (id.empty() || p.id != id || p.name.empty()) throw std::invalid_argument("Invalid label profile identity.");
    auto entry = LabelFieldsJson(p.fields);
    entry["id"] = id; entry["name"] = p.name;
    entry["metrics"] = "sceneMetres";
    // Validate mutations with the same rules as imported fields.
    if (entry.contains("presentation")) ReadLabelFields(entry);
    j.push_back(std::move(entry));
  }
  return j;
}
inline FixtureLabelProfiles ReadLabelProfiles(const nlohmann::json &j) {
  FixtureLabelProfiles profiles;
  if (!j.is_array()) throw std::invalid_argument("Invalid label profiles.");
  for (const auto &entry : j) {
    FixtureLabelStyleProfile p{entry.at("id").get<std::string>(), entry.at("name").get<std::string>(), {}};
    if (entry.at("metrics") != "sceneMetres") throw std::invalid_argument("Unsupported label profile metrics.");
    if (entry.contains("presentation")) p.fields = ReadLabelFields(entry);
    if (!profiles.emplace(p.id, p).second) throw std::invalid_argument("Duplicate label profile ID.");
  }
  LabelProfilesJson(profiles);
  return profiles;
}
} // namespace layouts
