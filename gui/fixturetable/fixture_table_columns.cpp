#include "fixture_table_columns.h"

#include "colorfulrenderers.h"
#include "columnutils.h"

namespace FixtureTableColumns {

std::vector<wxString> DefaultLabels() {
  return {"Fixture ID", "Name",        "Type",      "Layer",
          "Hang Pos",   "Universe",    "Channel",   "Mode",
          "Ch Count",   "Model file",  "Pos X",     "Pos Y",
          "Pos Z",      "Roll (X)",    "Pitch (Y)", "Yaw (Z)",
          "Power (W)",  "Weight (kg)", "Color"};
}

void ConfigureColumns(wxDataViewListCtrl *table,
                      const std::vector<wxString> &columnLabels) {
  std::vector<int> widths = {90, 150, 180, 100, 120, 80, 80,  120, 80, 180,
                             80, 80,  80,  80,  80,  80, 100, 100, 80};
  int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;

  auto *idRenderer =
      new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
  table->AppendColumn(new wxDataViewColumn(columnLabels[0], idRenderer, 0,
                                           widths[0], wxALIGN_LEFT, flags));

  table->AppendColumn(new wxDataViewColumn(
      columnLabels[1], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                wxALIGN_LEFT),
      1, widths[1], wxALIGN_LEFT, flags));

  table->AppendColumn(new wxDataViewColumn(
      columnLabels[2], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                wxALIGN_LEFT),
      2, widths[2], wxALIGN_LEFT, flags));

  table->AppendColumn(new wxDataViewColumn(
      columnLabels[3], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                wxALIGN_LEFT),
      3, widths[3], wxALIGN_LEFT, flags));

  table->AppendColumn(new wxDataViewColumn(
      columnLabels[4], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                wxALIGN_LEFT),
      4, widths[4], wxALIGN_LEFT, flags));

  table->AppendColumn(new wxDataViewColumn(
      columnLabels[5], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                wxALIGN_LEFT),
      5, widths[5], wxALIGN_LEFT, flags));

  table->AppendColumn(new wxDataViewColumn(
      columnLabels[6], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                wxALIGN_LEFT),
      6, widths[6], wxALIGN_LEFT, flags));

  for (size_t i = 7; i < columnLabels.size() - 1; ++i)
    table->AppendColumn(new wxDataViewColumn(
        columnLabels[i], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                  wxALIGN_LEFT),
        i, widths[i], wxALIGN_LEFT, flags));

  auto *colorRenderer =
      new ColorfulIconTextRenderer(wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
  table->AppendColumn(new wxDataViewColumn(
      columnLabels.back(), colorRenderer, columnLabels.size() - 1,
      widths.back(), wxALIGN_LEFT, flags));

  ColumnUtils::EnforceMinColumnWidth(table);
}

} // namespace FixtureTableColumns
