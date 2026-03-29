#pragma once

#include "configmanager.h"

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace viewer2d {

struct FixtureLabelOverride {
  std::array<std::optional<bool>, 3> showLabelName;
  std::array<std::optional<bool>, 3> showLabelId;
  std::array<std::optional<bool>, 3> showLabelDmx;
  std::array<std::optional<float>, 3> labelOffsetDistance;
  std::array<std::optional<float>, 3> labelOffsetAngle;
  std::optional<float> labelFontSizeName;
  std::optional<float> labelFontSizeId;
  std::optional<float> labelFontSizeDmx;

  bool HasAnyValue() const;
};

using FixtureLabelOverrideMap =
    std::unordered_map<std::string, FixtureLabelOverride>;

FixtureLabelOverrideMap LoadFixtureLabelOverrides(const ConfigManager &cfg);
void SaveFixtureLabelOverrides(ConfigManager &cfg,
                               const FixtureLabelOverrideMap &overrides);

float ResolveLabelFontSizeName(const ConfigManager &cfg,
                               const FixtureLabelOverride *overrideSettings);
float ResolveLabelFontSizeId(const ConfigManager &cfg,
                             const FixtureLabelOverride *overrideSettings);
float ResolveLabelFontSizeDmx(const ConfigManager &cfg,
                              const FixtureLabelOverride *overrideSettings);

bool ResolveShowLabelName(const ConfigManager &cfg,
                          const FixtureLabelOverride *overrideSettings,
                          int viewIndex);
bool ResolveShowLabelId(const ConfigManager &cfg,
                        const FixtureLabelOverride *overrideSettings,
                        int viewIndex);
bool ResolveShowLabelDmx(const ConfigManager &cfg,
                         const FixtureLabelOverride *overrideSettings,
                         int viewIndex);
float ResolveLabelOffsetDistance(const ConfigManager &cfg,
                                 const FixtureLabelOverride *overrideSettings,
                                 int viewIndex);
float ResolveLabelOffsetAngle(const ConfigManager &cfg,
                              const FixtureLabelOverride *overrideSettings,
                              int viewIndex);

void ApplyShowLabelNameOverride(ConfigManager &cfg,
                                const std::vector<std::string> &fixtureUuids,
                                int viewIndex, bool value);
void ApplyShowLabelIdOverride(ConfigManager &cfg,
                              const std::vector<std::string> &fixtureUuids,
                              int viewIndex, bool value);
void ApplyShowLabelDmxOverride(ConfigManager &cfg,
                               const std::vector<std::string> &fixtureUuids,
                               int viewIndex, bool value);
void ApplyLabelOffsetDistanceOverride(
    ConfigManager &cfg, const std::vector<std::string> &fixtureUuids,
    int viewIndex, float value);
void ApplyLabelOffsetAngleOverride(ConfigManager &cfg,
                                   const std::vector<std::string> &fixtureUuids,
                                   int viewIndex, float value);
void ApplyLabelFontSizeNameOverride(ConfigManager &cfg,
                                    const std::vector<std::string> &fixtureUuids,
                                    float value);
void ApplyLabelFontSizeIdOverride(ConfigManager &cfg,
                                  const std::vector<std::string> &fixtureUuids,
                                  float value);
void ApplyLabelFontSizeDmxOverride(ConfigManager &cfg,
                                   const std::vector<std::string> &fixtureUuids,
                                   float value);

} // namespace viewer2d
