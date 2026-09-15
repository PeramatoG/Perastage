#pragma once

#include "selection_interaction_policy.h"

#include <string>

class MvrScene;

namespace viewer3d::interaction {

// Builds typed table highlights for an item and the members of its group.
TypedSelection BuildHoverSelection(const MvrScene &scene,
                                   const std::string &hoverUuid);

// Builds the typed selection represented by clicking an item or its group.
TypedSelection BuildClickSelection(const MvrScene &scene,
                                   const std::string &clickedUuid);

} // namespace viewer3d::interaction
