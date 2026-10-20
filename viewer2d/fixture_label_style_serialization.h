#pragma once
#include "fixture_label_style.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>

namespace viewer2d {
inline void ReadFieldStyles(const nlohmann::json &json, LabelFieldStyles &styles) {
  const auto it = json.find("presentation");
  if (it == json.end() || !it->is_object()) return;
  const auto version = it->find("version");
  if (version == it->end() || !version->is_number_integer() || *version != 1) return;
  auto a = it->find("fields");
  if (a == it->end() || !a->is_array()) return;
  for (size_t v = 0; v < 3 && v < a->size(); ++v) {
    if (!(*a)[v].is_array()) continue;
    for (size_t f = 0; f < 3 && f < (*a)[v].size(); ++f) {
      const auto &j = (*a)[v][f];
      if (!j.is_object()) continue;
      try {
        LabelFieldStyle s;
        s.visible = j.value("visible", true); s.bold = j.value("bold", false);
        s.size = j.value("sizeMetres", 0.12f);
        s.x = j.value("xMetres", 0.f); s.y = j.value("yMetres", 0.f);
        s.anchor = j.value("anchor", 1); s.alignment = j.value("alignment", 1);
        s.color = j.value("color", s.color);
        if (!std::isfinite(s.size) || s.size < 0.004 || s.size > 4 ||
            !std::isfinite(s.x) || !std::isfinite(s.y) ||
            std::abs(s.x) > 100 || std::abs(s.y) > 100 ||
            s.anchor < 0 || s.anchor > 4 || s.alignment < 0 || s.alignment > 2 ||
            !std::all_of(s.color.begin(), s.color.end(), [](float c) { return std::isfinite(c) && c >= 0 && c <= 1; })) continue;
        styles[v][f] = s;
      } catch (const nlohmann::json::exception &) { }
    }
  }
}
inline void WriteFieldStyles(nlohmann::json &json, const LabelFieldStyles &styles) {
  nlohmann::json a = nlohmann::json::array();
  bool any = false;
  for (const auto &view : styles) {
    nlohmann::json row = nlohmann::json::array();
    for (const auto &s : view) {
      if (!s) { row.push_back(nullptr); continue; }
      any = true;
      row.push_back({{"visible", s->visible}, {"bold", s->bold}, {"sizeMetres", s->size},
        {"xMetres", s->x}, {"yMetres", s->y}, {"anchor", s->anchor},
        {"alignment", s->alignment}, {"color", s->color}});
    }
    a.push_back(row);
  }
  if (any) json["presentation"] = {{"version", 1}, {"fields", a}};
}
} // namespace viewer2d
