/* This file is part of Perastage, licensed under GPL-3.0-or-later. */
#include "gdtf_download_queue_table.h"
#include <wx/translation.h>
#include <vector>

class GdtfDownloadQueueTable::Model final : public wxDataViewIndexListModel {
public:
  struct Row {
    std::array<wxString, 5> text;
    wxColour colour;
    std::array<bool, 3> available{};
    int selected = -1;
    bool enabled = false;
  };
  std::vector<Row> rows;
  unsigned GetColumnCount() const override { return 8; }
  wxString GetColumnType(unsigned column) const override {
    return column < 5 ? wxS("string") : wxS("bool");
  }
  void GetValueByRow(wxVariant &value, unsigned row, unsigned column) const override {
    if (column < 5)
      value = rows[row].text[column];
    else
      value = rows[row].selected == static_cast<int>(column - 5);
  }
  bool SetValueByRow(const wxVariant &value, unsigned row, unsigned column) override {
    if (column < 5 || column >= 8 || !IsEnabledByRow(row, column) || !value.GetBool())
      return false;
    // Checking one source clears the other two; a source cannot be unchecked alone.
    rows[row].selected = static_cast<int>(column - 5);
    return true;
  }
  bool GetAttrByRow(unsigned row, unsigned, wxDataViewItemAttr &attr) const override {
    if (!rows[row].colour.IsOk())
      return false;
    attr.SetColour(rows[row].colour);
    return true;
  }
  bool IsEnabledByRow(unsigned row, unsigned column) const override {
    return column < 5 || (column < 8 && rows[row].enabled && rows[row].available[column - 5]);
  }
};

GdtfDownloadQueueTable::GdtfDownloadQueueTable(wxWindow *parent)
    : wxDataViewCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES) {
  model = new Model;
  AssociateModel(model);
  model->DecRef();
  AppendTextColumn(_("Fixture type"), 0, wxDATAVIEW_CELL_INERT, 235);
  AppendTextColumn(_("Selected GDTF"), 1, wxDATAVIEW_CELL_INERT, 340);
  AppendTextColumn(_("Status"), 2, wxDATAVIEW_CELL_INERT, 215);
  AppendTextColumn(_("Progress"), 3, wxDATAVIEW_CELL_INERT, 100);
  AppendTextColumn(_("Details"), 4, wxDATAVIEW_CELL_INERT, 160);
  Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, [this](wxDataViewEvent &event) {
    // Value-change events identify model columns even when view columns are reordered.
    const int index = event.GetColumn();
    if (updating || index < 5 || index >= 8 || !selectSource || !event.GetItem().IsOk())
      return;
    selectSource(model->GetRow(event.GetItem()), index - 5);
  });
}

long GdtfDownloadQueueTable::InsertItem(long row, const wxString &type) {
  wxCHECK_MSG(row == static_cast<long>(model->rows.size()), -1, "Queue rows must append");
  Model::Row value;
  value.text[0] = type;
  model->rows.push_back(std::move(value));
  model->RowAppended();
  return row;
}
void GdtfDownloadQueueTable::SetItem(long row, int column, const wxString &text) {
  if (model->rows[row].text[column] == text)
    return;
  model->rows[row].text[column] = text;
  updating = true;
  model->RowChanged(row);
  updating = false;
}
wxString GdtfDownloadQueueTable::GetItemText(long row, int column) const {
  return model->rows[row].text[column];
}
void GdtfDownloadQueueTable::SetItemTextColour(long row, const wxColour &colour) {
  model->rows[row].colour = colour;
  updating = true;
  model->RowChanged(row);
  updating = false;
}
wxColour GdtfDownloadQueueTable::GetItemTextColour(long row) const {
  return model->rows[row].colour;
}
unsigned GdtfDownloadQueueTable::GetItemCount() const { return model->rows.size(); }
void GdtfDownloadQueueTable::SelectRow(unsigned row) { Select(model->GetItem(row)); }
int GdtfDownloadQueueTable::SelectedRow() const {
  const auto item = GetSelection();
  return item.IsOk() ? static_cast<int>(model->GetRow(item)) : -1;
}
void GdtfDownloadQueueTable::EnableSourceChoices(std::function<void(unsigned, unsigned)> select) {
  selectSource = std::move(select);
  GetColumn(1)->SetWidth(275);
  GetColumn(2)->SetWidth(210);
  GetColumn(3)->SetWidth(95);
  GetColumn(4)->SetHidden(true); // The selected-row pane presents these details.
  const std::array<wxString, 3> labels = {_("MVR"), _("Perastage"), _("Selected file")};
  const auto valueType = wxS("bool");
  for (unsigned source = 0; source < 3; ++source)
    InsertColumn(1 + source, new wxDataViewColumn(labels[source],
        new wxDataViewToggleRenderer(valueType, wxDATAVIEW_CELL_ACTIVATABLE),
        5 + source, source == 0 ? 65 : source == 1 ? 85 : 105, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE));
}
void GdtfDownloadQueueTable::SetSourceChoices(unsigned row, const std::array<bool, 3> &available,
                                             int selected, bool enabled) {
  auto &value = model->rows[row];
  value.available = available;
  value.selected = selected;
  value.enabled = enabled;
  updating = true;
  model->RowChanged(row);
  updating = false;
}
bool GdtfDownloadQueueTable::SourceEnabled(unsigned row, unsigned source) const {
  return model->IsEnabledByRow(row, 5 + source);
}
bool GdtfDownloadQueueTable::SourceChecked(unsigned row, unsigned source) const {
  return model->rows[row].selected == static_cast<int>(source);
}
