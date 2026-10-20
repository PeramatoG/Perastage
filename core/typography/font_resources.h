#pragma once
#include "font_metrics.h"
#include <filesystem>
#include <stdexcept>

namespace typography {
inline std::filesystem::path BundledFontPath(const std::filesystem::path &root, bool bold) {
  return root / "fonts" / (bold ? "PerastageSans-Bold.ttf" : "PerastageSans.ttf");
}
struct FontPair {
  TtfFontMetrics regular, bold;
  explicit FontPair(const std::filesystem::path &root) {
    if (!LoadTtfFontMetrics(BundledFontPath(root, false), regular) ||
        !LoadTtfFontMetrics(BundledFontPath(root, true), bold))
      throw std::runtime_error("Bundled PerastageSans fonts are unavailable");
  }
};
} // namespace typography
