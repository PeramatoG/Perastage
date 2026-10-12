#pragma once
#include "json.hpp"
#include "mvrscene.h"
#include <cmath>
#include <string>

namespace fixture_power {
inline constexpr const char *kProjectKey = "fixture_planning_power_overrides";
// PSTG configuration only: never private extensions inside standard GDTF/MVR.
inline std::string SerializeOverrides(const MvrScene &scene) {
  auto values = nlohmann::json::object();
  for (const auto &[uuid, fixture] : scene.fixtures)
    if (fixture.planningPowerOverrideW)
      values[uuid] = *fixture.planningPowerOverrideW;
  return values.dump();
}
inline void RestoreOverrides(MvrScene &scene, const std::string &text) {
  const auto values = nlohmann::json::parse(text, nullptr, false);
  if (!values.is_object()) return;
  for (auto &[uuid, fixture] : scene.fixtures) {
    auto value = values.find(uuid);
    if (value == values.end() || !value->is_number()) continue;
    const float watts = value->get<float>();
    if (std::isfinite(watts) && watts >= 0) fixture.SetPlanningPowerOverride(watts);
  }
}
}
