#pragma once

#include "fixturetablepanel.h"

// Resolves the minimal scene update category for a fixture table column.
FixtureTablePanel::SceneDataUpdateType UpdateTypeForColumnImpl(int column);

// Merges two update categories into the most conservative required update.
FixtureTablePanel::SceneDataUpdateType CombineUpdateTypesImpl(
    FixtureTablePanel::SceneDataUpdateType lhs,
    FixtureTablePanel::SceneDataUpdateType rhs);
