#pragma once

#include "continuous_placement_type.h"

#include <array>
#include <string>

class MvrScene;

namespace continuous_placement {

bool Contains(const MvrScene &scene, ContinuousPlacementType type,
              const std::string &uuid);
bool CloneElement(MvrScene &scene, ContinuousPlacementType type,
                  const std::string &sourceUuid, const std::string &nextUuid);
void EraseElement(MvrScene &scene, ContinuousPlacementType type,
                  const std::string &uuid);
std::array<float, 3> PositionMeters(const MvrScene &scene,
                                    ContinuousPlacementType type,
                                    const std::string &uuid);
const char *ElementName(ContinuousPlacementType type);

} // namespace continuous_placement
