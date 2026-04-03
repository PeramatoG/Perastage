#include "hoist_load_limit_utils.h"

namespace HoistLoadLimitUtils {

namespace {
constexpr float kNearCapacityMarginKg = 75.0f;
constexpr float kToleranceKg = 0.001f;
}

LoadLimitState Evaluate(const float loadKg, const float capacityKg) {
  if (capacityKg <= 0.0f)
    return LoadLimitState::Normal;

  if (loadKg + kToleranceKg >= capacityKg)
    return LoadLimitState::AtOrAboveCapacity;

  if (loadKg >= capacityKg - kNearCapacityMarginKg)
    return LoadLimitState::NearCapacity;

  return LoadLimitState::Normal;
}

std::string TooltipForState(const LoadLimitState state) {
  switch (state) {
  case LoadLimitState::NearCapacity:
    return "Load is within 75 kg of hoist capacity.";
  case LoadLimitState::AtOrAboveCapacity:
    return "Load meets or exceeds hoist capacity.";
  default:
    return std::string();
  }
}

} // namespace HoistLoadLimitUtils
