#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

namespace model_detail {
enum class Level { Low, Standard, High };
inline constexpr const char *kDetailConfigKey = "viewer3d_model_detail";
inline constexpr const char *kMovingProxyConfigKey = "viewer3d_moving_proxy";

struct Preferences {
  Level level = Level::Standard;
  bool movingProxy = false;
  bool operator==(const Preferences &) const = default;
};

inline Level ParseLevel(const std::optional<std::string> &value) {
  if (value == "low")
    return Level::Low;
  if (value == "high")
    return Level::High;
  return Level::Standard;
}
inline const char *ConfigValue(Level level) {
  switch (level) {
  case Level::Low:
    return "low";
  case Level::High:
    return "high";
  default:
    return "standard";
  }
}
template <typename Store> Preferences ReadPreferences(const Store &store) {
  return {ParseLevel(store.GetValue(kDetailConfigKey)),
          store.GetValue(kMovingProxyConfigKey) == "1"};
}
template <typename Store>
void SavePreferences(Store &store, Preferences preferences) {
  store.SetValue(kDetailConfigKey, ConfigValue(preferences.level));
  store.SetValue(kMovingProxyConfigKey, preferences.movingProxy ? "1" : "0");
}

// Preserve raw optional values through project reset/import without migrating
// storage or turning missing defaults into explicit user choices.
struct PreferenceSnapshot {
  std::optional<std::string> detail;
  std::optional<std::string> proxy;
  template <typename Store> void Restore(Store &store) const {
    if (detail) store.SetValue(kDetailConfigKey, *detail);
    else store.RemoveKey(kDetailConfigKey);
    if (proxy) store.SetValue(kMovingProxyConfigKey, *proxy);
    else store.RemoveKey(kMovingProxyConfigKey);
  }
};
template <typename Store>
PreferenceSnapshot CapturePreferences(const Store &store) {
  return {store.GetValue(kDetailConfigKey), store.GetValue(kMovingProxyConfigKey)};
}

struct SimplificationPolicy {
  size_t targetTriangles;
  float maxRelativeError;
  bool allowCoarseFallback;
};
inline constexpr float kOverdrawThreshold = 1.05f;
inline constexpr float kProxyFallbackErrors[] = {0.01f, 0.02f, 0.05f, 0.10f, 0.20f};

// The temporary proxy may tolerate 25% target slack or 80% of the source.
inline size_t ProxyAcceptableIndexCount(size_t source, size_t target) {
  return std::max(target + target / 4, std::min(source, source * 4 / 5));
}
inline bool ProxyNeedsCoarseFallback(size_t generated, size_t source,
                                    size_t acceptable) {
  return generated < 3 || generated >= source * 95 / 100 || generated > acceptable;
}

// Budgets are best-effort for persistent detail: preserve constrained geometry
// rather than punch holes. Only the optional temporary proxy permits coarse fallback.
inline SimplificationPolicy ResolvePolicy(size_t triangles, Level level,
                                          bool movingProxy) {
  if (!movingProxy && (level == Level::High ||
      triangles <= (level == Level::Standard ? 2000u : 500u)))
    return {triangles, 0.0f, false};
  if (movingProxy && triangles <= 500)
    return {triangles, 0.0f, false};
  float ratio;
  if (movingProxy || level == Level::Low) {
    ratio = triangles <= 2000 ? 0.60f : triangles <= 10000 ? 0.35f
          : triangles <= 50000 ? 0.20f : triangles <= 200000 ? 0.10f : 0.05f;
  } else {
    ratio = triangles <= 10000 ? 0.65f : triangles <= 50000 ? 0.50f : 0.40f;
  }
  const size_t minimum = level == Level::Standard && !movingProxy ? 1000 : 300;
  const size_t maximum = level == Level::Standard && !movingProxy ? 100000 : 25000;
  return {std::clamp(static_cast<size_t>(triangles * ratio),
                     std::min(minimum, triangles), std::min(maximum, triangles)),
          level == Level::Standard && !movingProxy ? 0.002f : 0.01f,
          movingProxy};
}
} // namespace model_detail
