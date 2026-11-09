/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include "fixture_patch_address.h"
#include "patchmanager.h"
#include <string>
namespace perastage::patch {
// Explicit single starts do not undergo automatic placement. Intentional batches
// retain the existing sequential algorithm and its universe wrapping.
inline std::vector<PatchManager::PatchAddress> PlanTablePatch(
    const std::vector<int> &counts, int universe, int channel) {
  if (!ParseFixturePatchAddress(std::to_string(universe) + "." + std::to_string(channel)))
    return {};
  if (counts.size() == 1) return {{universe, channel}};
  return PatchManager::SequentialPatch(counts, universe, channel);
}
}
