#pragma once

#include <string>

namespace GdtfFixtureCategory {

inline constexpr const char *kUnknown = "Unknown";
inline constexpr const char *kHoist = "Hoist";
inline constexpr const char *kSmoke = "Smoke";
inline constexpr const char *kLaser = "Laser";
inline constexpr const char *kVideo = "Video";
inline constexpr const char *kFx = "FX";
inline constexpr const char *kStrobe = "Strobe";
inline constexpr const char *kLed = "LED";
inline constexpr const char *kConventional = "Conventional";
inline constexpr const char *kWash = "Wash";
inline constexpr const char *kSpot = "Spot";
inline constexpr const char *kBeam = "Beam";
inline constexpr const char *kHybrid = "Hybrid";

inline constexpr const char *kManualSource = "Manual";
inline constexpr const char *kAutoFallbackSource = "AutoFallback";

struct InferenceResult {
  std::string category;
  std::string reason;
};

bool IsValidCategory(const std::string &category);
std::string NormalizeCategory(const std::string &category);
InferenceResult InferFromGdtf(const std::string &gdtfPath);

} // namespace GdtfFixtureCategory
