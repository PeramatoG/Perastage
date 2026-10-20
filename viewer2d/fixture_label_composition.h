#pragma once
#include "fixture_label_overrides.h"
#include "canvas2d.h"
#include "typography/font_resources.h"

namespace viewer2d {
struct LabelBounds {
  double minX = 0, minY = 0, maxX = 0, maxY = 0;
  bool operator==(const LabelBounds &) const = default;
};
struct FixtureLabelComposition {
  struct Field { size_t first = 0, count = 0; LabelBounds bounds; };
  std::array<std::optional<Field>,3> fields;
  std::vector<TextCommand> text;
  std::vector<std::string> diagnostics;
};
bool HasFieldPresentation(const FixtureLabelOverrideMap &all,
                         const std::string &uuid, int view, int field);
FixtureLabelComposition ComposeFixtureLabels(const ConfigManager &cfg,
    const FixtureLabelOverrideMap &all, const Fixture &fixture, int view,
    const LabelBounds &bounds, const typography::FontPair &fonts);

// Owned by the existing per-renderer label cache, never by project state.
class FixtureLabelCompositionCache {
public:
  const FixtureLabelComposition &Resolve(const ConfigManager &cfg,
      const FixtureLabelOverrideMap &all, const Fixture &fixture, int view,
      const LabelBounds &bounds, const typography::FontPair &fonts);
  void Clear() { entries_.clear(); }
private:
  struct Key {
    std::string name, address;
    int id, view;
    LabelBounds bounds;
    std::array<std::optional<LabelFieldStyle>, 3> styles;
    bool operator==(const Key &) const = default;
  };
  struct Entry { Key key; FixtureLabelComposition composition; };
  std::filesystem::path resourceRoot_;
  std::unordered_map<std::string, Entry> entries_;
};
} // namespace viewer2d
