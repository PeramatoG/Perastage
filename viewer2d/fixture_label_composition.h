#pragma once
#include "fixture_label_overrides.h"
#include "canvas2d.h"
#include "typography/font_resources.h"

namespace viewer2d {
struct LabelBounds { double minX = 0, minY = 0, maxX = 0, maxY = 0; };
struct FixtureLabelComposition {
  std::vector<TextCommand> text;
  std::vector<std::string> diagnostics;
};
bool HasFieldPresentation(const FixtureLabelOverrideMap &all,
                         const std::string &uuid, int view, int field);
FixtureLabelComposition ComposeFixtureLabels(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const Fixture &fixture, int view,
    const LabelBounds &bounds, const typography::FontPair &fonts);
} // namespace viewer2d
