#pragma once

#include <array>
#include <cmath>

// Returns a display color [r, g, b] in 0..1 for a given 1-based DMX universe.
// Universe < 1 (no patch) returns white {1, 1, 1}.
// The first 16 universes are assigned hand-picked, highly distinct colors.
// Higher universes use a golden-angle HSV spiral for consistent generation.
inline std::array<float, 3> GetUniverseColor(int universe) {
  if (universe < 1)
    return {1.0f, 1.0f, 1.0f};

  // Hand-picked palette: first 16 universes are maximally distinct.
  static const std::array<std::array<float, 3>, 16> kPalette = {{
    {0.95f, 0.15f, 0.15f},  //  1: red
    {0.10f, 0.82f, 0.18f},  //  2: green
    {0.15f, 0.30f, 0.95f},  //  3: blue
    {0.95f, 0.88f, 0.08f},  //  4: yellow
    {0.05f, 0.88f, 0.88f},  //  5: cyan
    {0.90f, 0.12f, 0.90f},  //  6: magenta
    {0.96f, 0.50f, 0.08f},  //  7: orange
    {0.58f, 0.08f, 0.92f},  //  8: violet
    {0.08f, 0.55f, 0.28f},  //  9: forest green
    {0.10f, 0.55f, 0.95f},  // 10: sky blue
    {0.95f, 0.28f, 0.58f},  // 11: rose
    {0.72f, 0.95f, 0.08f},  // 12: lime
    {0.95f, 0.68f, 0.40f},  // 13: peach
    {0.38f, 0.08f, 0.60f},  // 14: indigo
    {0.08f, 0.78f, 0.60f},  // 15: teal
    {0.82f, 0.42f, 0.18f},  // 16: burnt orange
  }};

  if (universe <= 16)
    return kPalette[universe - 1];

  // Golden-angle HSV spiral for universes beyond 16.
  static constexpr float kGoldenAngle = 137.508f / 360.0f;
  float hue = std::fmod(static_cast<float>(universe - 1) * kGoldenAngle, 1.0f);
  float sat = 0.65f + static_cast<float>((universe * 37) % 30) / 100.0f;
  float val = 0.70f + static_cast<float>((universe * 53) % 25) / 100.0f;

  float c = val * sat;
  float hh = hue * 6.0f;
  float x = c * (1.0f - std::fabs(std::fmod(hh, 2.0f) - 1.0f));
  float r = 0.0f, g = 0.0f, b = 0.0f;
  if      (hh < 1.0f) { r = c; g = x; }
  else if (hh < 2.0f) { r = x; g = c; }
  else if (hh < 3.0f) {        g = c; b = x; }
  else if (hh < 4.0f) {        g = x; b = c; }
  else if (hh < 5.0f) { r = x;        b = c; }
  else                { r = c;        b = x; }
  float m = val - c;
  return {r + m, g + m, b + m};
}
