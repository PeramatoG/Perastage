#include <cassert>
#include <limits>

#include "rigging_weight_validation.h"

// Verifies invalid rigging weights are detected and excluded from totals.
int main() {
  float total = 0.0f;
  assert(gui::rigging::AccumulateValidPhysicalWeightKg(12.5f, total));
  assert(total == 12.5f);
  assert(!gui::rigging::AccumulateValidPhysicalWeightKg(0.0f, total));
  assert(!gui::rigging::AccumulateValidPhysicalWeightKg(-1.0f, total));
  assert(!gui::rigging::AccumulateValidPhysicalWeightKg(
      std::numeric_limits<float>::quiet_NaN(), total));
  assert(!gui::rigging::AccumulateValidPhysicalWeightKg(
      std::numeric_limits<float>::infinity(), total));
  assert(total == 12.5f);
}
