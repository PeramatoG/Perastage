#include "fixturetablepanel_update_types.h"

#include "fixturetablepanel.h"

// Resolves the minimal scene update category for a fixture table column.
FixtureTablePanel::SceneDataUpdateType UpdateTypeForColumnImpl(int column) {
  using SceneDataUpdateType = FixtureTablePanel::SceneDataUpdateType;
  switch (column) {
  case 1:
    return SceneDataUpdateType::kVisualLabelOnly;
  case 5:
  case 6:
  case 8:
    return SceneDataUpdateType::kPatchOnly;
  case 10:
  case 11:
  case 12:
  case 13:
  case 14:
  case 15:
    return SceneDataUpdateType::kTransformOnly;
  case 16:
  case 17:
    return SceneDataUpdateType::kWeightOrPosition;
  case 18:
    return SceneDataUpdateType::kCategoryOnly;
  case 19:
    return SceneDataUpdateType::kAppearanceOnly;
  case 0:
    return SceneDataUpdateType::kFixtureIdOnly;
  case 2:
  case 3:
  case 4:
  case 7:
  case 9:
    return SceneDataUpdateType::kMetadataOnly;
  default:
    return SceneDataUpdateType::kGeneral;
  }
}

// Merges two update categories into the most conservative required update.
FixtureTablePanel::SceneDataUpdateType CombineUpdateTypesImpl(
    FixtureTablePanel::SceneDataUpdateType lhs,
    FixtureTablePanel::SceneDataUpdateType rhs) {
  using SceneDataUpdateType = FixtureTablePanel::SceneDataUpdateType;
  if (lhs == rhs)
    return lhs;
  if (lhs == SceneDataUpdateType::kGeneral || rhs == SceneDataUpdateType::kGeneral)
    return SceneDataUpdateType::kGeneral;
  if (lhs == SceneDataUpdateType::kVisualLabelOnly)
    return rhs;
  if (rhs == SceneDataUpdateType::kVisualLabelOnly)
    return lhs;
  return SceneDataUpdateType::kGeneral;
}
