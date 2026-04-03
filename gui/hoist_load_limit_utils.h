#pragma once

#include <string>

namespace HoistLoadLimitUtils {

enum class LoadLimitState {
  Normal,
  NearCapacity,
  AtOrAboveCapacity,
};

LoadLimitState Evaluate(float loadKg, float capacityKg);

inline bool IsCritical(LoadLimitState state) {
  return state != LoadLimitState::Normal;
}

std::string TooltipForState(LoadLimitState state);

} // namespace HoistLoadLimitUtils
