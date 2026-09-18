#pragma once

#include "colorstore.h"
#include "fixturetable/fixture_table_columns.h"
#include "table_column_indices.h"

namespace DataViewSortProfiles {

// Configures semantic sorting for the fixture table's named columns.
inline void ConfigureFixture(ColorfulDataViewListStore &store) {
  using Column = FixtureTableColumns::Column;
  for (const Column column : {Column::FixtureId, Column::Universe,
                              Column::Channel, Column::ChannelCount})
    store.SetColumnSortMode(FixtureTableColumns::ToIndex(column),
                            DataViewSort::Mode::Integer);
  for (const Column column :
       {Column::PositionX, Column::PositionY, Column::PositionZ, Column::Roll,
        Column::Pitch, Column::Yaw, Column::Power, Column::Weight})
    store.SetColumnSortMode(FixtureTableColumns::ToIndex(column),
                            DataViewSort::Mode::Decimal);
  for (const Column column :
       {Column::Name, Column::Type, Column::Layer, Column::HangPosition,
        Column::Mode, Column::ModelFile})
    store.SetColumnSortMode(FixtureTableColumns::ToIndex(column),
                            DataViewSort::Mode::NaturalText);
}

// Configures semantic sorting for the truss table's named columns.
inline void ConfigureTruss(ColorfulDataViewListStore &store) {
  using Column = TrussTableColumns::Column;
  for (const Column column :
       {Column::PositionX, Column::PositionY, Column::PositionZ, Column::Roll,
        Column::Pitch, Column::Yaw, Column::Length, Column::Width,
        Column::Height, Column::Weight, Column::Load})
    store.SetColumnSortMode(TableColumnIndices::ToIndex(column),
                            DataViewSort::Mode::Decimal);
  for (const Column column :
       {Column::Name, Column::Layer, Column::ModelFile, Column::HangPosition,
        Column::Manufacturer, Column::Model})
    store.SetColumnSortMode(TableColumnIndices::ToIndex(column),
                            DataViewSort::Mode::NaturalText);
}

// Configures semantic sorting for the hoist table's named columns.
inline void ConfigureHoist(ColorfulDataViewListStore &store) {
  using Column = HoistTableColumns::Column;
  store.SetColumnSortMode(TableColumnIndices::ToIndex(Column::HoistId),
                          DataViewSort::Mode::Integer);
  for (const Column column :
       {Column::PositionX, Column::PositionY, Column::PositionZ, Column::Roll,
        Column::Pitch, Column::Yaw, Column::ChainLength, Column::Capacity,
        Column::Weight, Column::Load})
    store.SetColumnSortMode(TableColumnIndices::ToIndex(column),
                            DataViewSort::Mode::Decimal);
  for (const Column column :
       {Column::Name, Column::Type, Column::Function, Column::Motor,
        Column::DummyPreset, Column::Layer, Column::HangPosition})
    store.SetColumnSortMode(TableColumnIndices::ToIndex(column),
                            DataViewSort::Mode::NaturalText);
}

// Configures semantic sorting for the scene-object table's named columns.
inline void ConfigureSceneObject(ColorfulDataViewListStore &store) {
  using Column = SceneObjectTableColumns::Column;
  for (const Column column :
       {Column::PositionX, Column::PositionY, Column::PositionZ, Column::Roll,
        Column::Pitch, Column::Yaw})
    store.SetColumnSortMode(TableColumnIndices::ToIndex(column),
                            DataViewSort::Mode::Decimal);
  for (const Column column : {Column::Name, Column::Layer, Column::ModelFile})
    store.SetColumnSortMode(TableColumnIndices::ToIndex(column),
                            DataViewSort::Mode::NaturalText);
}

// Configures natural sorting for human-readable dictionary identifiers.
inline void ConfigureDictionaries(ColorfulDataViewListStore &fixtureStore,
                                  ColorfulDataViewListStore &trussStore) {
  using FixtureColumn = DictionaryFixtureTableColumns::Column;
  using TrussColumn = DictionaryTrussTableColumns::Column;
  for (const FixtureColumn column :
       {FixtureColumn::Name, FixtureColumn::File, FixtureColumn::Mode})
    fixtureStore.SetColumnSortMode(TableColumnIndices::ToIndex(column),
                                   DataViewSort::Mode::NaturalText);
  for (const TrussColumn column : {TrussColumn::Name, TrussColumn::File})
    trussStore.SetColumnSortMode(TableColumnIndices::ToIndex(column),
                                 DataViewSort::Mode::NaturalText);
}

} // namespace DataViewSortProfiles
