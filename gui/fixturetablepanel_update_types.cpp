#include "fixturetablepanel_update_types.h"

#include "fixturetable/fixture_table_columns.h"
#include "fixturetablepanel.h"

// Resolves the minimal scene update category for a fixture table column.
FixtureTablePanel::SceneDataUpdateType UpdateTypeForColumnImpl(int column) {
  using SceneDataUpdateType = FixtureTablePanel::SceneDataUpdateType;
  const auto namedColumn = FixtureTableColumns::FromIndex(column);
  if (!namedColumn)
    return SceneDataUpdateType::kGeneral;
  using Column = FixtureTableColumns::Column;
  switch (*namedColumn) {
  case Column::Name:
    return SceneDataUpdateType::kVisualLabelOnly;
  case Column::Universe:
  case Column::Channel:
  case Column::ChannelCount:
    return SceneDataUpdateType::kPatchOnly;
  case Column::PositionX:
  case Column::PositionY:
  case Column::PositionZ:
  case Column::Roll:
  case Column::Pitch:
  case Column::Yaw:
    return SceneDataUpdateType::kTransformOnly;
  case Column::Power:
  case Column::Weight:
    return SceneDataUpdateType::kWeightOrPosition;
  case Column::Category:
    return SceneDataUpdateType::kCategoryOnly;
  case Column::VisualColor:
    return SceneDataUpdateType::kAppearanceOnly;
  case Column::FixtureId:
    return SceneDataUpdateType::kFixtureIdOnly;
  case Column::Type:
  case Column::Layer:
  case Column::HangPosition:
  case Column::Mode:
  case Column::ModelFile:
    return SceneDataUpdateType::kMetadataOnly;
  case Column::Count:
    return SceneDataUpdateType::kGeneral;
  }
  return SceneDataUpdateType::kGeneral;
}

// Merges two update categories into the most conservative required update.
FixtureTablePanel::SceneDataUpdateType
CombineUpdateTypesImpl(FixtureTablePanel::SceneDataUpdateType lhs,
                       FixtureTablePanel::SceneDataUpdateType rhs) {
  using SceneDataUpdateType = FixtureTablePanel::SceneDataUpdateType;
  if (lhs == rhs)
    return lhs;
  if (lhs == SceneDataUpdateType::kGeneral ||
      rhs == SceneDataUpdateType::kGeneral)
    return SceneDataUpdateType::kGeneral;
  if (lhs == SceneDataUpdateType::kVisualLabelOnly)
    return rhs;
  if (rhs == SceneDataUpdateType::kVisualLabelOnly)
    return lhs;
  return SceneDataUpdateType::kGeneral;
}
