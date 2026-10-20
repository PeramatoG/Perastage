#include "fixture_label_overrides.h"
#include <algorithm>

namespace viewer2d {
LabelFieldStyle ResolveFieldStyle(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const std::string &uuid, int view, int field) {
  view = std::clamp(view, 0, 2); field = std::clamp(field, 0, 2);
  const auto it = all.find(uuid);
  const auto *legacy = it == all.end() ? nullptr : &it->second;
  if (legacy && legacy->fieldStyles[view][field]) return *legacy->fieldStyles[view][field];
  const auto defaults = all.find(kLabelDefaultsKey);
  if (defaults != all.end() && defaults->second.fieldStyles[view][field]) {
    auto s = *defaults->second.fieldStyles[view][field];
    if (legacy) {
      const auto &show = field == 0 ? legacy->showLabelName : field == 1 ? legacy->showLabelId : legacy->showLabelDmx;
      const auto &size = field == 0 ? legacy->labelFontSizeName : field == 1 ? legacy->labelFontSizeId : legacy->labelFontSizeDmx;
      if (show[view]) s.visible = *show[view];
      if (size) s.size = *size / 25.f;
      if (legacy->labelOffsetDistance[view] || legacy->labelOffsetAngle[view]) {
        const float angle = ResolveLabelOffsetAngle(cfg, legacy, view) * 3.14159265358979323846f / 180;
        const float distance = ResolveLabelOffsetDistance(cfg, legacy, view);
        s.x = distance * std::sin(angle); s.y = distance * std::cos(angle) - field * s.size;
      }
    }
    return s;
  }
  LabelFieldStyle s;
  s.visible = field == 0 ? ResolveShowLabelName(cfg, legacy, view) :
              field == 1 ? ResolveShowLabelId(cfg, legacy, view) : ResolveShowLabelDmx(cfg, legacy, view);
  s.size = (field == 0 ? ResolveLabelFontSizeName(cfg, legacy) :
            field == 1 ? ResolveLabelFontSizeId(cfg, legacy) : ResolveLabelFontSizeDmx(cfg, legacy)) / 25.f;
  s.bold = field == 2;
  const float radians = ResolveLabelOffsetAngle(cfg, legacy, view) * 3.14159265358979323846f / 180;
  const float distance = ResolveLabelOffsetDistance(cfg, legacy, view);
  s.x = distance * std::sin(radians);
  s.y = distance * std::cos(radians) - field * s.size;
  return s;
}

bool CommitFixtureLabelStyles(ConfigManager &cfg, const FixtureLabelOverrideMap &candidate) {
  const auto after = SerializeFixtureLabelOverrides(candidate);
  if (SerializeFixtureLabelOverrides(LoadFixtureLabelOverrides(cfg)) == after) return false;
  return cfg.CommitFixtureLabelMutation(after, "Edit fixture labels");
}
} // namespace viewer2d
