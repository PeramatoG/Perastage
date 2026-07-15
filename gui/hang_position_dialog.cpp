#include "hang_position_dialog.h"

#include "fixturetable/fixture_table_columns.h"
#include "fixturetablepanel.h"
#include "hoisttablepanel.h"
#include "table_column_indices.h"
#include "trusstablepanel.h"

#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <set>

namespace {

// Returns the model column that stores fixture hang position values.
constexpr int FixtureHangPositionColumn() {
  return FixtureTableColumns::ToIndex(FixtureTableColumns::Column::HangPosition);
}

// Returns the model column that stores truss hang position values.
constexpr int TrussHangPositionColumn() {
  return TableColumnIndices::ToIndex(TrussTableColumns::Column::HangPosition);
}

// Returns the model column that stores hoist hang position values.
constexpr int HoistHangPositionColumn() {
  return TableColumnIndices::ToIndex(HoistTableColumns::Column::HangPosition);
}

// Normalizes a hang position entered by the user.
wxString NormalizeHangPosition(const wxString &value) {
  wxString normalized = value;
  return normalized.Trim(true).Trim(false);
}

// Adds non-empty hang positions from a table column to the shared set.
void CollectFromTable(wxDataViewListCtrl *table, int column,
                      std::set<wxString> *positions) {
  if (!table || !positions)
    return;

  const unsigned int count = table->GetItemCount();
  for (unsigned int row = 0; row < count; ++row) {
    wxVariant value;
    table->GetValue(value, row, column);
    wxString position = NormalizeHangPosition(value.GetString());
    if (!position.empty())
      positions->insert(position);
  }
}

// Collects hang positions currently visible in all open rigging tables.
std::vector<wxString> CollectSharedHangPositions() {
  std::set<wxString> positions;
  if (auto *panel = FixtureTablePanel::Instance())
    CollectFromTable(panel->GetTableCtrl(), FixtureHangPositionColumn(),
                     &positions);
  if (auto *panel = TrussTablePanel::Instance())
    CollectFromTable(panel->GetTableCtrl(), TrussHangPositionColumn(),
                     &positions);
  if (auto *panel = HoistTablePanel::Instance())
    CollectFromTable(panel->GetTableCtrl(), HoistHangPositionColumn(),
                     &positions);
  return {positions.begin(), positions.end()};
}

// Sets the list box contents and preserves the requested selection when possible.
void PopulateList(wxListBox *list, const std::vector<wxString> &positions,
                  const wxString &selection) {
  list->Clear();
  for (const wxString &position : positions)
    list->Append(position);
  const int selectedIndex = list->FindString(selection, true);
  if (selectedIndex != wxNOT_FOUND)
    list->SetSelection(selectedIndex);
  else if (!positions.empty())
    list->SetSelection(0);
}

// Replaces table values using accepted rename and delete operations.
bool ApplyDictionaryChanges(wxDataViewListCtrl *table, int column,
                            const HangPositionDialogResult &result) {
  if (!table)
    return false;

  bool changed = false;
  const unsigned int count = table->GetItemCount();
  for (unsigned int row = 0; row < count; ++row) {
    wxVariant current;
    table->GetValue(current, row, column);
    const wxString oldValue = NormalizeHangPosition(current.GetString());
    wxString newValue = oldValue;

    auto renameIt = result.renamedPositions.find(oldValue);
    if (renameIt != result.renamedPositions.end())
      newValue = renameIt->second;
    if (result.deletedPositions.find(oldValue) != result.deletedPositions.end())
      newValue.clear();

    if (newValue != oldValue) {
      table->SetValue(wxVariant(newValue), row, column);
      changed = true;
    }
  }
  return changed;
}

// Sets selected rows in a table to the selected hang position.
bool ApplySelectedRows(wxDataViewListCtrl *table, int column,
                       const std::vector<unsigned int> &rows,
                       const wxString &selectedName) {
  if (!table)
    return false;

  bool changed = false;
  const unsigned int count = table->GetItemCount();
  for (unsigned int row : rows) {
    if (row >= count)
      continue;
    wxVariant current;
    table->GetValue(current, row, column);
    if (NormalizeHangPosition(current.GetString()) != selectedName) {
      table->SetValue(wxVariant(selectedName), row, column);
      changed = true;
    }
  }
  return changed;
}

class HangPositionDialog : public wxDialog {
public:
  HangPositionDialog(wxWindow *parent, const wxString &currentName);
  HangPositionDialogResult Result() const;

private:
  void OnAdd(wxCommandEvent &event);
  void OnRename(wxCommandEvent &event);
  void OnDelete(wxCommandEvent &event);

  wxListBox *positionList = nullptr;
  std::vector<wxString> positions;
  std::map<wxString, wxString> renamedPositions;
  std::set<wxString> deletedPositions;
};

// Builds the shared hang position editor dialog.
HangPositionDialog::HangPositionDialog(wxWindow *parent,
                                       const wxString &currentName)
    : wxDialog(parent, wxID_ANY, _("Hang Position"), wxDefaultPosition,
               wxSize(420, 360), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      positions(CollectSharedHangPositions()) {
  const wxString normalizedCurrent = NormalizeHangPosition(currentName);
  if (!normalizedCurrent.empty() &&
      std::find(positions.begin(), positions.end(), normalizedCurrent) ==
          positions.end())
    positions.push_back(normalizedCurrent);
  std::sort(positions.begin(), positions.end());

  auto *mainSizer = new wxBoxSizer(wxVERTICAL);
  mainSizer->Add(new wxStaticText(this, wxID_ANY,
                                  _("Select or manage a shared hang position:")),
                 0, wxALL, 8);

  positionList = new wxListBox(this, wxID_ANY);
  PopulateList(positionList, positions, normalizedCurrent);
  mainSizer->Add(positionList, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

  auto *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  auto *addButton = new wxButton(this, wxID_ADD, _("Add"));
  auto *renameButton = new wxButton(this, wxID_ANY, _("Rename"));
  auto *deleteButton = new wxButton(this, wxID_DELETE, _("Delete"));
  buttonSizer->Add(addButton, 0, wxRIGHT, 6);
  buttonSizer->Add(renameButton, 0, wxRIGHT, 6);
  buttonSizer->Add(deleteButton, 0);
  mainSizer->Add(buttonSizer, 0, wxALL, 8);

  mainSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                 wxEXPAND | wxALL, 8);
  SetSizerAndFit(mainSizer);

  addButton->Bind(wxEVT_BUTTON, &HangPositionDialog::OnAdd, this);
  renameButton->Bind(wxEVT_BUTTON, &HangPositionDialog::OnRename, this);
  deleteButton->Bind(wxEVT_BUTTON, &HangPositionDialog::OnDelete, this);
}

// Returns the dialog result after the user accepts the changes.
HangPositionDialogResult HangPositionDialog::Result() const {
  HangPositionDialogResult result;
  const int selection = positionList->GetSelection();
  if (selection != wxNOT_FOUND)
    result.selectedName = positionList->GetString(selection);
  result.renamedPositions = renamedPositions;
  result.deletedPositions = deletedPositions;
  return result;
}

// Adds a new hang position to the shared list.
void HangPositionDialog::OnAdd(wxCommandEvent &event) {
  wxTextEntryDialog dialog(this, _("Enter hang position name:"),
                           _("Add Hang Position"));
  if (dialog.ShowModal() != wxID_OK)
    return;

  const wxString value = NormalizeHangPosition(dialog.GetValue());
  if (value.empty())
    return;
  if (std::find(positions.begin(), positions.end(), value) == positions.end()) {
    positions.push_back(value);
    std::sort(positions.begin(), positions.end());
  }
  deletedPositions.erase(value);
  PopulateList(positionList, positions, value);
}

// Renames the currently selected hang position in the shared list.
void HangPositionDialog::OnRename(wxCommandEvent &event) {
  const int selection = positionList->GetSelection();
  if (selection == wxNOT_FOUND)
    return;

  const wxString oldValue = positionList->GetString(selection);
  wxTextEntryDialog dialog(this, _("Enter hang position name:"),
                           _("Rename Hang Position"), oldValue);
  if (dialog.ShowModal() != wxID_OK)
    return;

  const wxString newValue = NormalizeHangPosition(dialog.GetValue());
  if (newValue.empty() || newValue == oldValue)
    return;

  positions.erase(std::remove(positions.begin(), positions.end(), oldValue),
                  positions.end());
  if (std::find(positions.begin(), positions.end(), newValue) == positions.end())
    positions.push_back(newValue);
  std::sort(positions.begin(), positions.end());

  for (auto &rename : renamedPositions) {
    if (rename.second == oldValue)
      rename.second = newValue;
  }
  renamedPositions[oldValue] = newValue;
  deletedPositions.erase(oldValue);
  deletedPositions.erase(newValue);
  PopulateList(positionList, positions, newValue);
}

// Deletes the currently selected hang position from the shared list.
void HangPositionDialog::OnDelete(wxCommandEvent &event) {
  const int selection = positionList->GetSelection();
  if (selection == wxNOT_FOUND)
    return;

  const wxString oldValue = positionList->GetString(selection);
  if (wxMessageBox(_("Delete this hang position from all affected items?"),
                   _("Delete Hang Position"),
                   wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
    return;

  positions.erase(std::remove(positions.begin(), positions.end(), oldValue),
                  positions.end());
  renamedPositions.erase(oldValue);
  for (auto it = renamedPositions.begin(); it != renamedPositions.end();) {
    if (it->second == oldValue) {
      deletedPositions.insert(it->first);
      it = renamedPositions.erase(it);
    } else {
      ++it;
    }
  }
  deletedPositions.insert(oldValue);
  PopulateList(positionList, positions, wxString());
}

} // namespace

// Shows the shared hang position editor and returns accepted changes.
bool ShowHangPositionDialog(wxWindow *parent, const wxString &currentName,
                            HangPositionDialogResult *result) {
  HangPositionDialog dialog(parent, currentName);
  if (dialog.ShowModal() != wxID_OK)
    return false;
  if (result)
    *result = dialog.Result();
  return true;
}

// Applies accepted hang position changes to all open rigging tables.
void ApplySharedHangPositionChanges(
    const HangPositionDialogResult &result, bool updateFixtures,
    const std::vector<unsigned int> &fixtureRows, bool updateTrusses,
    const std::vector<unsigned int> &trussRows, bool updateHoists,
    const std::vector<unsigned int> &hoistRows) {
  bool fixtureChanged = false;
  bool trussChanged = false;
  bool hoistChanged = false;

  if (auto *panel = FixtureTablePanel::Instance()) {
    auto *table = panel->GetTableCtrl();
    fixtureChanged =
        ApplyDictionaryChanges(table, FixtureHangPositionColumn(), result);
    if (updateFixtures)
      fixtureChanged = ApplySelectedRows(table, FixtureHangPositionColumn(),
                                         fixtureRows, result.selectedName) ||
                       fixtureChanged;
    if (fixtureChanged)
      panel->UpdateSceneData(true,
                             FixtureTablePanel::SceneDataUpdateType::kMetadataOnly);
  }

  if (auto *panel = TrussTablePanel::Instance()) {
    auto *table = panel->GetTableCtrl();
    trussChanged = ApplyDictionaryChanges(table, TrussHangPositionColumn(),
                                          result);
    if (updateTrusses)
      trussChanged = ApplySelectedRows(table, TrussHangPositionColumn(), trussRows,
                                       result.selectedName) ||
                     trussChanged;
    if (trussChanged)
      panel->UpdateSceneData();
  }

  if (auto *panel = HoistTablePanel::Instance()) {
    auto *table = panel->GetTableCtrl();
    hoistChanged = ApplyDictionaryChanges(table, HoistHangPositionColumn(),
                                          result);
    if (updateHoists)
      hoistChanged = ApplySelectedRows(table, HoistHangPositionColumn(), hoistRows,
                                       result.selectedName) ||
                     hoistChanged;
    if (hoistChanged)
      panel->UpdateSceneData();
  }
}
