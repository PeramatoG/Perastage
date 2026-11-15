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
  FixtureLabelOverride inherited = legacy ? *legacy : FixtureLabelOverride{};
  if (defaults != all.end()) {
    const auto inherit = [](auto &value, const auto &fallback) { if (!value) value = fallback; };
    for (int v=0; v<3; ++v) {
      inherit(inherited.showLabelName[v], defaults->second.showLabelName[v]);
      inherit(inherited.showLabelId[v], defaults->second.showLabelId[v]);
      inherit(inherited.showLabelDmx[v], defaults->second.showLabelDmx[v]);
      inherit(inherited.labelOffsetDistance[v], defaults->second.labelOffsetDistance[v]);
      inherit(inherited.labelOffsetAngle[v], defaults->second.labelOffsetAngle[v]);
    }
    inherit(inherited.labelFontSizeName, defaults->second.labelFontSizeName);
    inherit(inherited.labelFontSizeId, defaults->second.labelFontSizeId);
    inherit(inherited.labelFontSizeDmx, defaults->second.labelFontSizeDmx);
  }
  const auto *effective = &inherited;
  if (defaults != all.end() && defaults->second.fieldStyles[view][field]) {
    auto s = *defaults->second.fieldStyles[view][field];
    if (legacy) {
      const auto &show = field == 0 ? legacy->showLabelName : field == 1 ? legacy->showLabelId : legacy->showLabelDmx;
      const auto &size = field == 0 ? legacy->labelFontSizeName : field == 1 ? legacy->labelFontSizeId : legacy->labelFontSizeDmx;
      if (show[view]) s.visible = *show[view];
      if (size) s.size = *size / 25.f;
      if (legacy->labelOffsetDistance[view] || legacy->labelOffsetAngle[view]) {
        const float angle = ResolveLabelOffsetAngle(cfg, effective, view) * 3.14159265358979323846f / 180;
        const float distance = ResolveLabelOffsetDistance(cfg, effective, view);
        s.x = distance * std::sin(angle); s.y = distance * std::cos(angle) - field * s.size;
      }
    }
    return s;
  }
  LabelFieldStyle s;
  if (cfg.GetFloat("view2d_dark_mode") != 0) {
    s.color = {1,1,1};
    s.outlineColor = {0,0,0};
  }
  s.visible = field == 0 ? ResolveShowLabelName(cfg, effective, view) :
              field == 1 ? ResolveShowLabelId(cfg, effective, view) : ResolveShowLabelDmx(cfg, effective, view);
  s.size = (field == 0 ? ResolveLabelFontSizeName(cfg, effective) :
            field == 1 ? ResolveLabelFontSizeId(cfg, effective) : ResolveLabelFontSizeDmx(cfg, effective)) / 25.f;
  s.bold = field == 2;
  const float radians = ResolveLabelOffsetAngle(cfg, effective, view) * 3.14159265358979323846f / 180;
  const float distance = ResolveLabelOffsetDistance(cfg, effective, view);
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
