#include "fixture_label_overrides.h"

#include "json.hpp"

#include <algorithm>
#include <array>
#include <type_traits>

namespace viewer2d {
namespace {
constexpr const char *kFixtureOverridesKey = "label_fixture_overrides";
constexpr std::array<const char *, 3> kNameKeys = {"label_show_name_top",
                                                    "label_show_name_front",
                                                    "label_show_name_side"};
constexpr std::array<const char *, 3> kIdKeys = {"label_show_id_top",
                                                  "label_show_id_front",
                                                  "label_show_id_side"};
constexpr std::array<const char *, 3> kDmxKeys = {"label_show_dmx_top",
                                                   "label_show_dmx_front",
                                                   "label_show_dmx_side"};
constexpr std::array<const char *, 3> kDistanceKeys = {
    "label_offset_distance_top", "label_offset_distance_front",
    "label_offset_distance_side"};
constexpr std::array<const char *, 3> kAngleKeys = {"label_offset_angle_top",
                                                     "label_offset_angle_front",
                                                     "label_offset_angle_side"};

template <typename T>
void ReadOptionalArray(const nlohmann::json &obj, const char *key,
                       std::array<std::optional<T>, 3> &out) {
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_array())
    return;
  const auto &arr = *it;
  for (size_t i = 0; i < out.size() && i < arr.size(); ++i) {
    const auto &entry = arr[i];
    if constexpr (std::is_same_v<T, bool>) {
      if (entry.is_boolean())
        out[i] = entry.get<bool>();
    } else {
      if (entry.is_number())
        out[i] = entry.get<float>();
    }
  }
}

template <typename T>
nlohmann::json WriteOptionalArray(const std::array<std::optional<T>, 3> &values) {
  nlohmann::json arr = nlohmann::json::array();
  bool anyValue = false;
  for (const auto &value : values) {
    if (value.has_value()) {
      arr.push_back(*value);
      anyValue = true;
    } else {
      arr.push_back(nullptr);
    }
  }
  if (!anyValue)
    return {};
  return arr;
}

template <typename Setter>
void ApplyToSelection(ConfigManager &cfg,
                      const std::vector<std::string> &fixtureUuids,
                      Setter setter) {
  if (fixtureUuids.empty())
    return;

  auto overrides = LoadFixtureLabelOverrides(cfg);
  for (const auto &uuid : fixtureUuids) {
    if (uuid.empty())
      continue;
    setter(overrides[uuid]);
  }
  SaveFixtureLabelOverrides(cfg, overrides);
}
} // namespace

bool FixtureLabelOverride::HasAnyValue() const {
  auto hasAnyArrayValue = [](const auto &arr) {
    return std::any_of(arr.begin(), arr.end(), [](const auto &v) {
      return v.has_value();
    });
  };
  return hasAnyArrayValue(showLabelName) || hasAnyArrayValue(showLabelId) ||
         hasAnyArrayValue(showLabelDmx) ||
         hasAnyArrayValue(labelOffsetDistance) ||
         hasAnyArrayValue(labelOffsetAngle) || labelFontSizeName.has_value() ||
         labelFontSizeId.has_value() || labelFontSizeDmx.has_value();
}

FixtureLabelOverrideMap LoadFixtureLabelOverrides(const ConfigManager &cfg) {
  FixtureLabelOverrideMap overrides;
  const auto raw = cfg.GetValue(kFixtureOverridesKey);
  if (!raw || raw->empty())
    return overrides;

  const nlohmann::json root = nlohmann::json::parse(*raw, nullptr, false);
  if (!root.is_object())
    return overrides;

  for (auto it = root.begin(); it != root.end(); ++it) {
    if (!it.value().is_object())
      continue;
    FixtureLabelOverride entry;
    ReadOptionalArray<bool>(it.value(), "showLabelName", entry.showLabelName);
    ReadOptionalArray<bool>(it.value(), "showLabelId", entry.showLabelId);
    ReadOptionalArray<bool>(it.value(), "showLabelDmx", entry.showLabelDmx);
    ReadOptionalArray<float>(it.value(), "labelOffsetDistance",
                             entry.labelOffsetDistance);
    ReadOptionalArray<float>(it.value(), "labelOffsetAngle",
                             entry.labelOffsetAngle);
    if (auto fit = it.value().find("labelFontSizeName");
        fit != it.value().end() && fit->is_number()) {
      entry.labelFontSizeName = fit->get<float>();
    }
    if (auto fit = it.value().find("labelFontSizeId");
        fit != it.value().end() && fit->is_number()) {
      entry.labelFontSizeId = fit->get<float>();
    }
    if (auto fit = it.value().find("labelFontSizeDmx");
        fit != it.value().end() && fit->is_number()) {
      entry.labelFontSizeDmx = fit->get<float>();
    }
    if (entry.HasAnyValue())
      overrides[it.key()] = entry;
  }
  return overrides;
}

void SaveFixtureLabelOverrides(ConfigManager &cfg,
                               const FixtureLabelOverrideMap &overrides) {
  nlohmann::json root = nlohmann::json::object();
  for (const auto &[uuid, entry] : overrides) {
    if (!entry.HasAnyValue())
      continue;
    nlohmann::json fixtureJson = nlohmann::json::object();
    if (auto arr = WriteOptionalArray(entry.showLabelName); !arr.is_null())
      fixtureJson["showLabelName"] = arr;
    if (auto arr = WriteOptionalArray(entry.showLabelId); !arr.is_null())
      fixtureJson["showLabelId"] = arr;
    if (auto arr = WriteOptionalArray(entry.showLabelDmx); !arr.is_null())
      fixtureJson["showLabelDmx"] = arr;
    if (auto arr = WriteOptionalArray(entry.labelOffsetDistance); !arr.is_null())
      fixtureJson["labelOffsetDistance"] = arr;
    if (auto arr = WriteOptionalArray(entry.labelOffsetAngle); !arr.is_null())
      fixtureJson["labelOffsetAngle"] = arr;
    if (entry.labelFontSizeName.has_value())
      fixtureJson["labelFontSizeName"] = *entry.labelFontSizeName;
    if (entry.labelFontSizeId.has_value())
      fixtureJson["labelFontSizeId"] = *entry.labelFontSizeId;
    if (entry.labelFontSizeDmx.has_value())
      fixtureJson["labelFontSizeDmx"] = *entry.labelFontSizeDmx;
    if (!fixtureJson.empty())
      root[uuid] = std::move(fixtureJson);
  }

  if (root.empty()) {
    cfg.RemoveKey(kFixtureOverridesKey);
    return;
  }
  cfg.SetValue(kFixtureOverridesKey, root.dump());
}

float ResolveLabelFontSizeName(const ConfigManager &cfg,
                               const FixtureLabelOverride *overrideSettings) {
  if (overrideSettings && overrideSettings->labelFontSizeName.has_value())
    return *overrideSettings->labelFontSizeName;
  return cfg.GetFloat("label_font_size_name");
}

float ResolveLabelFontSizeId(const ConfigManager &cfg,
                             const FixtureLabelOverride *overrideSettings) {
  if (overrideSettings && overrideSettings->labelFontSizeId.has_value())
    return *overrideSettings->labelFontSizeId;
  return cfg.GetFloat("label_font_size_id");
}

float ResolveLabelFontSizeDmx(const ConfigManager &cfg,
                              const FixtureLabelOverride *overrideSettings) {
  if (overrideSettings && overrideSettings->labelFontSizeDmx.has_value())
    return *overrideSettings->labelFontSizeDmx;
  return cfg.GetFloat("label_font_size_dmx");
}

bool ResolveShowLabelName(const ConfigManager &cfg,
                          const FixtureLabelOverride *overrideSettings,
                          int viewIndex) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  if (overrideSettings && overrideSettings->showLabelName[clampedView].has_value())
    return *overrideSettings->showLabelName[clampedView];
  return cfg.GetFloat(kNameKeys[clampedView]) != 0.0f;
}

bool ResolveShowLabelId(const ConfigManager &cfg,
                        const FixtureLabelOverride *overrideSettings,
                        int viewIndex) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  if (overrideSettings && overrideSettings->showLabelId[clampedView].has_value())
    return *overrideSettings->showLabelId[clampedView];
  return cfg.GetFloat(kIdKeys[clampedView]) != 0.0f;
}

bool ResolveShowLabelDmx(const ConfigManager &cfg,
                         const FixtureLabelOverride *overrideSettings,
                         int viewIndex) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  if (overrideSettings && overrideSettings->showLabelDmx[clampedView].has_value())
    return *overrideSettings->showLabelDmx[clampedView];
  return cfg.GetFloat(kDmxKeys[clampedView]) != 0.0f;
}

float ResolveLabelOffsetDistance(const ConfigManager &cfg,
                                 const FixtureLabelOverride *overrideSettings,
                                 int viewIndex) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  if (overrideSettings &&
      overrideSettings->labelOffsetDistance[clampedView].has_value()) {
    return *overrideSettings->labelOffsetDistance[clampedView];
  }
  return cfg.GetFloat(kDistanceKeys[clampedView]);
}

float ResolveLabelOffsetAngle(const ConfigManager &cfg,
                              const FixtureLabelOverride *overrideSettings,
                              int viewIndex) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  if (overrideSettings && overrideSettings->labelOffsetAngle[clampedView].has_value())
    return *overrideSettings->labelOffsetAngle[clampedView];
  return cfg.GetFloat(kAngleKeys[clampedView]);
}

void ApplyShowLabelNameOverride(ConfigManager &cfg,
                                const std::vector<std::string> &fixtureUuids,
                                int viewIndex, bool value) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  ApplyToSelection(cfg, fixtureUuids,
                   [clampedView, value](FixtureLabelOverride &entry) {
                     entry.showLabelName[clampedView] = value;
                   });
}

void ApplyShowLabelIdOverride(ConfigManager &cfg,
                              const std::vector<std::string> &fixtureUuids,
                              int viewIndex, bool value) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  ApplyToSelection(cfg, fixtureUuids,
                   [clampedView, value](FixtureLabelOverride &entry) {
                     entry.showLabelId[clampedView] = value;
                   });
}

void ApplyShowLabelDmxOverride(ConfigManager &cfg,
                               const std::vector<std::string> &fixtureUuids,
                               int viewIndex, bool value) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  ApplyToSelection(cfg, fixtureUuids,
                   [clampedView, value](FixtureLabelOverride &entry) {
                     entry.showLabelDmx[clampedView] = value;
                   });
}

void ApplyLabelOffsetDistanceOverride(
    ConfigManager &cfg, const std::vector<std::string> &fixtureUuids,
    int viewIndex, float value) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  ApplyToSelection(cfg, fixtureUuids,
                   [clampedView, value](FixtureLabelOverride &entry) {
                     entry.labelOffsetDistance[clampedView] = value;
                   });
}

void ApplyLabelOffsetAngleOverride(ConfigManager &cfg,
                                   const std::vector<std::string> &fixtureUuids,
                                   int viewIndex, float value) {
  const int clampedView = std::clamp(viewIndex, 0, 2);
  ApplyToSelection(cfg, fixtureUuids,
                   [clampedView, value](FixtureLabelOverride &entry) {
                     entry.labelOffsetAngle[clampedView] = value;
                   });
}

void ApplyLabelFontSizeNameOverride(ConfigManager &cfg,
                                    const std::vector<std::string> &fixtureUuids,
                                    float value) {
  ApplyToSelection(cfg, fixtureUuids,
                   [value](FixtureLabelOverride &entry) {
                     entry.labelFontSizeName = value;
                   });
}

void ApplyLabelFontSizeIdOverride(ConfigManager &cfg,
                                  const std::vector<std::string> &fixtureUuids,
                                  float value) {
  ApplyToSelection(cfg, fixtureUuids,
                   [value](FixtureLabelOverride &entry) {
                     entry.labelFontSizeId = value;
                   });
}

void ApplyLabelFontSizeDmxOverride(ConfigManager &cfg,
                                   const std::vector<std::string> &fixtureUuids,
                                   float value) {
  ApplyToSelection(cfg, fixtureUuids,
                   [value](FixtureLabelOverride &entry) {
                     entry.labelFontSizeDmx = value;
                   });
}

} // namespace viewer2d
