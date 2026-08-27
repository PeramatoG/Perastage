#include "dictionary_export_conflict_dialog.h"

#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace {
constexpr unsigned int kKeepExistingColumn = 1;
constexpr unsigned int kUseNewColumn = 2;
} // namespace

DictionaryExportConflictDialog::DictionaryExportConflictDialog(
    wxWindow *parent, const wxString &title, std::vector<Item> items)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(980, 560),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      items_(std::move(items)) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  topSizer->Add(new wxStaticText(
                    this, wxID_ANY,
                    _("The following files already exist with different content.\n"
                    "Choose whether to keep the existing file or use the new exported file.")),
                0, wxALL, 10);

  table_ = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxDV_ROW_LINES | wxDV_VERT_RULES);
  table_->SetMinSize(wxSize(940, 460));
  const int textFlags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
  table_->AppendTextColumn(_("File"), wxDATAVIEW_CELL_INERT, 180, wxALIGN_LEFT,
                           textFlags);
  table_->AppendToggleColumn(_("Keep existing"), wxDATAVIEW_CELL_ACTIVATABLE, 120,
                             wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE);
  table_->AppendToggleColumn(_("Use new"), wxDATAVIEW_CELL_ACTIVATABLE, 120,
                             wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE);
  table_->AppendTextColumn(_("Existing path"), wxDATAVIEW_CELL_INERT, 250,
                           wxALIGN_LEFT, textFlags);
  table_->AppendTextColumn(_("New source"), wxDATAVIEW_CELL_INERT, 250,
                           wxALIGN_LEFT, textFlags);
  topSizer->Add(table_, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

  wxBoxSizer *actionsSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *selectExistingButton =
      new wxButton(this, wxID_ANY, _("Select all existing"));
  wxButton *selectNewButton = new wxButton(this, wxID_ANY, _("Select all new"));
  actionsSizer->Add(selectExistingButton, 0, wxRIGHT, 8);
  actionsSizer->Add(selectNewButton, 0, wxRIGHT, 8);
  actionsSizer->AddStretchSpacer(1);
  actionsSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                    wxALIGN_CENTER_VERTICAL);

  topSizer->Add(actionsSizer, 0, wxEXPAND | wxALL, 10);
  SetSizer(topSizer);
  SetMinSize(wxSize(1100, 720));
  SetSize(wxSize(1260, 820));
  Layout();

  RefreshTable();

  selectExistingButton->Bind(wxEVT_BUTTON,
                             &DictionaryExportConflictDialog::SetAllToExisting,
                             this);
  selectNewButton->Bind(wxEVT_BUTTON,
                        &DictionaryExportConflictDialog::SetAllToNew, this);
  table_->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED,
               &DictionaryExportConflictDialog::OnTableValueChanged, this);
}

const std::vector<DictionaryExportConflictDialog::Item> &
DictionaryExportConflictDialog::GetItems() const {
  return items_;
}

void DictionaryExportConflictDialog::RefreshTable() {
  if (!table_)
    return;

  table_->DeleteAllItems();
  for (const Item &item : items_) {
    wxVector<wxVariant> values;
    values.push_back(wxVariant(wxString::FromUTF8(item.file_name)));
    values.push_back(wxVariant(!item.use_new));
    values.push_back(wxVariant(item.use_new));
    values.push_back(wxVariant(wxString::FromUTF8(item.existing_path)));
    values.push_back(wxVariant(wxString::FromUTF8(item.new_source_path)));
    table_->AppendItem(values);
  }
}

void DictionaryExportConflictDialog::SetAllToExisting(wxCommandEvent &WXUNUSED(event)) {
  for (Item &item : items_)
    item.use_new = false;
  RefreshTable();
}

void DictionaryExportConflictDialog::SetAllToNew(wxCommandEvent &WXUNUSED(event)) {
  for (Item &item : items_)
    item.use_new = true;
  RefreshTable();
}

void DictionaryExportConflictDialog::OnTableValueChanged(wxDataViewEvent &event) {
  const wxDataViewItem dataViewItem = event.GetItem();
  if (!dataViewItem.IsOk())
    return;

  const int row = table_->ItemToRow(dataViewItem);
  if (row < 0 || static_cast<size_t>(row) >= items_.size())
    return;

  const unsigned int column = event.GetColumn();
  if (column == kKeepExistingColumn) {
    items_[static_cast<size_t>(row)].use_new = false;
    RefreshTable();
  } else if (column == kUseNewColumn) {
    items_[static_cast<size_t>(row)].use_new = true;
    RefreshTable();
  }
}
