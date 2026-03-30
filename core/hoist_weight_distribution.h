#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "mvrscene.h"

namespace HoistWeightDistribution {

float RoundUpToNextFiveKg(float valueKg);
float RoundRiggingTotalForHangPosition(float totalWeightKg);

std::unordered_map<std::string, bool>
BuildMissingWeightMapByHangPosition(const MvrScene &scene);

std::unordered_map<std::string, float>
BuildRoundedRiggingTotalByHangPosition(
    const MvrScene &scene,
    const std::unordered_map<std::string, float> &extraWeightKgByPosition = {});

void ApplyForImportedSupports(MvrScene &scene,
                              const std::vector<std::string> &importedSupportUuids,
                              const std::unordered_map<std::string, float>
                                  &roundedRiggingTotalByPosition);

} // namespace HoistWeightDistribution
