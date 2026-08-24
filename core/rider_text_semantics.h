#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace rider_text {

enum class Section {
  None,
  Lighting,
  Effects,
  LightingControl,
  Video,
  Rigging,
  Ignored,
};

// Classifies a complete rider line when it is a recognized section heading.
Section ClassifySectionHeader(std::string_view line);

// Returns the canonical hang for a complete position heading.
std::optional<std::string> ClassifyHangHeader(std::string_view line);

// Normalizes a hang or rigging target alias to its canonical name.
std::string NormalizeHangAlias(std::string_view value);

// Reports whether a line starts with a positive equipment quantity.
bool IsQuantityPrefixedLine(std::string_view line);

} // namespace rider_text
