/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
namespace fixture_patch_presentation {
// Hypothetical byte-continuous routing only; never a scene allocation or break.
inline std::string SpanRanges(int universe, int channel, int footprint) {
  if (universe < 1 || channel < 1 || channel > 512 || footprint < 1) return {};
  using I = std::int64_t;
  std::string result;
  auto append = [&](I u, I first, I lastU, I last) {
    if (!result.empty()) result += "; ";
    result += std::to_string(u) + "." + std::to_string(first) + "-" +
              std::to_string(lastU) + "." + std::to_string(last);
  };
  const I initial = std::min<I>(footprint, 513 - channel);
  append(universe, channel, universe, channel + initial - 1);
  I remaining = footprint - initial;
  I next = static_cast<I>(universe) + 1;
  const I full = remaining / 512;
  if (full) { append(next, 1, next + full - 1, 512); next += full; }
  remaining %= 512;
  if (remaining) append(next, 1, next, remaining);
  return result;
}
}
