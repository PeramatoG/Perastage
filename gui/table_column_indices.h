#pragma once

#include <cstddef>
#include <optional>
#include <type_traits>

namespace TableColumnIndices {

// Converts a table-specific column enum to its stable model index.
template <typename Column> constexpr int ToIndex(Column column) {
  static_assert(std::is_enum_v<Column>);
  return static_cast<int>(column);
}

// Returns the number of model columns represented by a table-specific enum.
template <typename Column> constexpr size_t Count() {
  static_assert(std::is_enum_v<Column>);
  return static_cast<size_t>(Column::Count);
}

// Converts a model index to a table-specific column when the index is valid.
template <typename Column> std::optional<Column> FromIndex(int index) {
  static_assert(std::is_enum_v<Column>);
  if (index < 0 || index >= ToIndex(Column::Count))
    return std::nullopt;
  return static_cast<Column>(index);
}

} // namespace TableColumnIndices

namespace SceneObjectTableColumns {
enum class Column : int {
  Name = 0,
  Layer,
  ModelFile,
  PositionX,
  PositionY,
  PositionZ,
  Roll,
  Pitch,
  Yaw,
  Count
};
} // namespace SceneObjectTableColumns

namespace TrussTableColumns {
enum class Column : int {
  Name = 0,
  Layer,
  ModelFile,
  HangPosition,
  PositionX,
  PositionY,
  PositionZ,
  Roll,
  Pitch,
  Yaw,
  Manufacturer,
  Model,
  Length,
  Width,
  Height,
  Weight,
  Load,
  Count
};
} // namespace TrussTableColumns

namespace HoistTableColumns {
enum class Column : int {
  HoistId = 0,
  Name,
  Type,
  Function,
  Motor,
  DummyPreset,
  DataSource,
  Layer,
  HangPosition,
  PositionX,
  PositionY,
  PositionZ,
  Roll,
  Pitch,
  Yaw,
  ChainLength,
  Capacity,
  Weight,
  Load,
  Count
};
} // namespace HoistTableColumns

namespace RiggingTableColumns {
enum class Column : int {
  Position = 0,
  Fixtures,
  Trusses,
  Hoists,
  FixtureWeight,
  TrussWeight,
  HoistWeight,
  ExtraWeight,
  TotalWeight,
  RoundedTotalWeight,
  Count
};
} // namespace RiggingTableColumns

namespace LayerTableColumns {
enum class Column : int { Visible = 0, Layer, Color, Count };
} // namespace LayerTableColumns

namespace FixtureSummaryTableColumns {
enum class Column : int { Visible = 0, CountValue, Type, Color, Count };
} // namespace FixtureSummaryTableColumns

namespace ObjectSummaryTableColumns {
enum class Column : int { CountValue = 0, Type, Count };
} // namespace ObjectSummaryTableColumns

namespace DictionaryFixtureTableColumns {
enum class Column : int { Name = 0, File, Mode, Category, VisualColor, Count };
} // namespace DictionaryFixtureTableColumns

namespace DictionaryTrussTableColumns {
enum class Column : int { Name = 0, File, Count };
} // namespace DictionaryTrussTableColumns
