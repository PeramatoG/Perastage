#pragma once
#include "typography/composition.h"
#include "fixture_label_keys.h"
#include <array>
#include <optional>

namespace viewer2d {
inline constexpr const char *kLabelDefaultsKey = fixture_labels::kProjectDefaultsEntry;
struct LabelFieldStyle {
  bool visible = true;
  bool bold = false;
  float size = 0.12f; // Scene metres, independent of zoom and DPI.
  std::array<float, 3> color{0, 0, 0};
  int anchor = 1; // Center, top, bottom, left, right of projected fixture bounds.
  int alignment = 1;
  float x = 0, y = 0;
  bool operator==(const LabelFieldStyle &) const = default;
};
using LabelFieldStyles = std::array<std::array<std::optional<LabelFieldStyle>, 3>, 3>;
} // namespace viewer2d
