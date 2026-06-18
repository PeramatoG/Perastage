#pragma once

#include <cstddef>
#include <optional>
#include <vector>
#include <wx/dataview.h>
#include <wx/string.h>

class ColorfulDataViewListStore;

namespace FixtureTableColumns {

enum class Column : int {
  FixtureId = 0,
  Name,
  Type,
  Layer,
  HangPosition,
  Universe,
  Channel,
  Mode,
  ChannelCount,
  ModelFile,
  PositionX,
  PositionY,
  PositionZ,
  Roll,
  Pitch,
  Yaw,
  Power,
  Weight,
  Category,
  VisualColor,
  MvrColor,
  Count
};

constexpr int ToIndex(Column column) { return static_cast<int>(column); }
constexpr size_t Count() { return static_cast<size_t>(Column::Count); }

std::optional<Column> FromIndex(int index);
std::vector<wxString> DefaultLabels();
std::vector<int> DefaultWidths();
wxString Label(Column column);
int Width(Column column);
bool IsInteger(Column column);
bool IsNumeric(Column column);
bool IsPosition(Column column);
bool IsRotation(Column column);
bool IsTransform(Column column);
bool IsPhysicalProperty(Column column);
bool IsTypeLevelPropagated(Column column);
bool IsVisualColor(Column column);
bool IsMvrColor(Column column);
bool IsCategory(Column column);
bool IsPatch(Column column);
void ConfigureColumns(wxDataViewListCtrl *table,
                      const std::vector<wxString> &columnLabels);

} // namespace FixtureTableColumns
