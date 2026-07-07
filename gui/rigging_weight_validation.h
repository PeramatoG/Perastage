#pragma once

#include <cmath>

namespace gui {
namespace rigging {

// Reports whether a physical weight is usable for rigging totals.
inline bool IsValidPhysicalWeightKg(float weightKg) {
  return std::isfinite(weightKg) && weightKg > 0.0f;
}

// Adds a physical weight to a total only when it is valid for rigging.
inline bool AccumulateValidPhysicalWeightKg(float weightKg, float &totalKg) {
  if (!IsValidPhysicalWeightKg(weightKg))
    return false;
  totalKg += weightKg;
  return true;
}

} // namespace rigging
} // namespace gui
