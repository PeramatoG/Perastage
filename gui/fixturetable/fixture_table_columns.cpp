#include "fixture_table_columns.h"

#include "colorfulrenderers.h"
#include "columnutils.h"

#include <array>
#include <wx/intl.h>

namespace FixtureTableColumns {
namespace {
constexpr std::array<const char *, Count()> kLabels = {
    "Fixture ID",  "Name",      "Type",        "Layer",    "Hang Pos",
    "Universe",    "Channel",   "Mode",        "Ch Count", "Model file",
    "Pos X",       "Pos Y",     "Pos Z",       "Roll (X)", "Pitch (Y)",
    "Yaw (Z)",     "Power (W)", "Weight (kg)", "Category", "Type Color",
    "Color Filter"};
constexpr std::array<int, Count()> kWidths = {90,  150, 180, 100, 120, 80, 80,
                                              120, 80,  180, 80,  80,  80, 80,
                                              80,  80,  100, 100, 120, 90, 90};
static_assert(kLabels.size() == Count());
static_assert(kWidths.size() == Count());
} // namespace

// Converts a model index to a fixture column when the index is valid.
std::optional<Column> FromIndex(int index) {
  if (index < 0 || index >= ToIndex(Column::Count))
    return std::nullopt;
  return static_cast<Column>(index);
}

// Returns the default visible labels in stable fixture model order.
std::vector<wxString> DefaultLabels() {
  std::vector<wxString> labels;
  labels.reserve(kLabels.size());
  for (const char *label : kLabels)
    labels.push_back(wxGetTranslation(wxString::FromUTF8(label)));
  return labels;
}

// Returns the default visible widths in stable fixture model order.
std::vector<int> DefaultWidths() { return {kWidths.begin(), kWidths.end()}; }

// Returns the default label for a fixture column.
wxString Label(Column column) {
  return wxGetTranslation(
      wxString::FromUTF8(kLabels[static_cast<size_t>(ToIndex(column))]));
}

// Returns the default width for a fixture column.
int Width(Column column) {
  return kWidths[static_cast<size_t>(ToIndex(column))];
}

// Checks whether a fixture column accepts integer values.
bool IsInteger(Column column) {
  return column == Column::FixtureId || column == Column::Universe ||
         column == Column::Channel;
}

// Checks whether a fixture column accepts numeric values.
bool IsNumeric(Column column) {
  return IsPosition(column) || IsRotation(column) || IsPhysicalProperty(column);
}

// Checks whether a fixture column stores a position coordinate.
bool IsPosition(Column column) {
  return column >= Column::PositionX && column <= Column::PositionZ;
}

// Checks whether a fixture column stores a rotation angle.
bool IsRotation(Column column) {
  return column >= Column::Roll && column <= Column::Yaw;
}

// Checks whether a fixture column stores transform data.
bool IsTransform(Column column) {
  return IsPosition(column) || IsRotation(column);
}

// Checks whether a fixture column stores a physical property.
bool IsPhysicalProperty(Column column) {
  return column == Column::Power || column == Column::Weight;
}

// Checks whether edits propagate to fixtures sharing the same type.
bool IsTypeLevelPropagated(Column column) {
  return IsPhysicalProperty(column) || IsCategory(column) ||
         IsVisualColor(column);
}

// Checks whether a fixture column stores the Perastage visual color.
bool IsVisualColor(Column column) { return column == Column::VisualColor; }

// Checks whether a fixture column stores the official MVR Fixture/Color.
bool IsMvrColor(Column column) { return column == Column::MvrColor; }

// Checks whether a fixture column stores the fixture category.
bool IsCategory(Column column) { return column == Column::Category; }

// Checks whether a fixture column contributes to patch data.
bool IsPatch(Column column) {
  return column == Column::Universe || column == Column::Channel ||
         column == Column::ChannelCount;
}

// Configures fixture columns with stable model indexes and renderers.
void ConfigureColumns(wxDataViewListCtrl *table,
                      const std::vector<wxString> &columnLabels) {
  if (!table || columnLabels.size() != Count())
    return;
  const std::vector<int> widths = DefaultWidths();
  int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;

  auto *idRenderer =
      new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
  const int idColumn = ToIndex(Column::FixtureId);
  table->AppendColumn(new wxDataViewColumn(
      columnLabels[static_cast<size_t>(idColumn)], idRenderer, idColumn,
      widths[static_cast<size_t>(idColumn)], wxALIGN_LEFT, flags));

  for (int index = ToIndex(Column::Name); index < ToIndex(Column::VisualColor);
       ++index) {
    const size_t i = static_cast<size_t>(index);
    table->AppendColumn(new wxDataViewColumn(
        columnLabels[i],
        new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT, wxALIGN_LEFT), index,
        widths[i], wxALIGN_LEFT, flags));
  }

  for (const Column column : {Column::VisualColor, Column::MvrColor}) {
    auto *colorRenderer =
        new ColorfulIconTextRenderer(wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
    colorRenderer->EnableEllipsize(wxELLIPSIZE_NONE);
    const int colorColumn = ToIndex(column);
    table->AppendColumn(new wxDataViewColumn(
        columnLabels[static_cast<size_t>(colorColumn)], colorRenderer,
        colorColumn, widths[static_cast<size_t>(colorColumn)], wxALIGN_LEFT,
        flags));
  }

  ColumnUtils::EnforceMinColumnWidth(table);
}

} // namespace FixtureTableColumns
