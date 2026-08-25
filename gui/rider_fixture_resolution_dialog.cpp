#include "rider_fixture_resolution_dialog.h"

#include "gdtfsearchdialog.h"

#include <algorithm>
#include <sstream>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace {

// Joins position labels for compact table presentation.
wxString JoinPositions(const std::vector<std::string> &positions) {
  wxString value;
  for (const std::string &position : positions) {
    if (!value.empty())
      value += ", ";
    value += wxString::FromUTF8(position);
  }
  return value;
}

// Formats the selected or suggested catalog identity for a resolution row.
wxString FormatCatalogIdentity(const rider_fixture_resolution::Item &item) {
  const auto &entry = item.selectedEntry ? item.selectedEntry : item.suggestedEntry;
  if (!entry)
    return "-";
  if (entry->manufacturer.empty())
    return wxString::FromUTF8(entry->fixtureName);
  return wxString::FromUTF8(entry->manufacturer + " / " + entry->fixtureName);
}

} // namespace

// Creates the modal fixture-resolution review without starting downloads.
RiderFixtureResolutionDialog::RiderFixtureResolutionDialog(
    wxWindow *parent, rider_fixture_resolution::Analysis analysisIn,
    std::string catalogPayloadIn, std::string catalogUpdatedAtIn,
    CatalogLoader catalogLoaderIn)
    : wxDialog(parent, wxID_ANY, _("Resolve fixture types"), wxDefaultPosition,
               wxSize(1160, 680),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      analysis(std::move(analysisIn)),
      catalogPayload(std::move(catalogPayloadIn)),
      catalogUpdatedAt(std::move(catalogUpdatedAtIn)),
      catalogLoader(std::move(catalogLoaderIn)) {
  BuildLayout();
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
  SetMinSize(wxSize(780, 500));
  CentreOnParent();
}

// Returns the user-reviewed resolution model after the modal dialog succeeds.
rider_fixture_resolution::Analysis RiderFixtureResolutionDialog::TakeAnalysis() {
  return std::move(analysis);
}

// Builds the wide queue-style table, row actions, and footer controls.
void RiderFixtureResolutionDialog::BuildLayout() {
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(
                this, wxID_ANY,
                _("Review unknown rider fixture types before creating the scene.")),
            0, wxEXPAND | wxALL, 12);

  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxDV_ROW_LINES);
  const int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
  table->AppendTextColumn(_("Fixture type"), wxDATAVIEW_CELL_INERT, 220,
                          wxALIGN_LEFT, flags);
  table->AppendTextColumn(_("Qty"), wxDATAVIEW_CELL_INERT, 55, wxALIGN_RIGHT,
                          flags);
  table->AppendTextColumn(_("Positions"), wxDATAVIEW_CELL_INERT, 140,
                          wxALIGN_LEFT, flags);
  table->AppendTextColumn(_("Selected GDTF"), wxDATAVIEW_CELL_INERT, 270,
                          wxALIGN_LEFT, flags);
  table->AppendTextColumn(_("Mode"), wxDATAVIEW_CELL_INERT, 130, wxALIGN_LEFT,
                          flags);
  table->AppendTextColumn(_("Status"), wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT,
                          flags);
  table->AppendTextColumn(_("Details"), wxDATAVIEW_CELL_INERT, 220,
                          wxALIGN_LEFT, flags);
  root->Add(table, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);

  auto *actions = new wxBoxSizer(wxHORIZONTAL);
  useSuggestedButton = new wxButton(this, wxID_ANY, _("Use suggested"));
  searchButton = new wxButton(this, wxID_ANY, _("Search..."));
  useGenericButton = new wxButton(this, wxID_ANY, _("Use generic"));
  modeChoice = new wxChoice(this, wxID_ANY);
  actions->Add(useSuggestedButton, 0, wxRIGHT, 8);
  actions->Add(searchButton, 0, wxRIGHT, 8);
  actions->Add(useGenericButton, 0, wxRIGHT, 16);
  actions->Add(new wxStaticText(this, wxID_ANY, _("Mode:")), 0,
               wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  actions->Add(modeChoice, 1);
  root->Add(actions, 0, wxEXPAND | wxALL, 12);

  auto *footer = new wxBoxSizer(wxHORIZONTAL);
  auto *acceptAllButton =
      new wxButton(this, wxID_ANY, _("Accept all suggestions"));
  summaryLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
  resolveButton = new wxButton(this, wxID_OK, _("Resolve and create"));
  auto *cancelButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
  footer->Add(acceptAllButton, 0, wxRIGHT, 12);
  footer->Add(summaryLabel, 1, wxALIGN_CENTER_VERTICAL);
  footer->Add(resolveButton, 0, wxRIGHT, 8);
  footer->Add(cancelButton, 0);
  root->Add(footer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
  SetSizer(root);

  table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
              &RiderFixtureResolutionDialog::OnSelectionChanged, this);
  useSuggestedButton->Bind(wxEVT_BUTTON,
                           &RiderFixtureResolutionDialog::OnUseSuggested, this);
  searchButton->Bind(wxEVT_BUTTON, &RiderFixtureResolutionDialog::OnSearch,
                     this);
  useGenericButton->Bind(wxEVT_BUTTON,
                         &RiderFixtureResolutionDialog::OnUseGeneric, this);
  acceptAllButton->Bind(wxEVT_BUTTON,
                        &RiderFixtureResolutionDialog::OnAcceptAll, this);
  modeChoice->Bind(wxEVT_CHOICE,
                   &RiderFixtureResolutionDialog::OnModeSelected, this);
  resolveButton->Bind(wxEVT_BUTTON, &RiderFixtureResolutionDialog::OnResolve,
                      this);
}

// Rebuilds table rows from the current resolution model.
void RiderFixtureResolutionDialog::RefreshTable() {
  const int selectedRow = table->GetSelectedRow();
  table->DeleteAllItems();
  for (const auto &item : analysis.items) {
    wxVector<wxVariant> row;
    row.push_back(wxString::FromUTF8(item.request.typeName));
    row.push_back(wxString::Format("%d", item.request.quantity));
    row.push_back(JoinPositions(item.request.positions));
    row.push_back(FormatCatalogIdentity(item));
    row.push_back(item.selectedMode.empty()
                      ? wxString("-")
                      : wxString::FromUTF8(item.selectedMode));
    row.push_back(wxString::FromUTF8(rider_fixture_resolution::StateName(item.state)));
    row.push_back(wxString::FromUTF8(item.details));
    table->AppendItem(row);
  }
  if (!analysis.items.empty())
    table->SelectRow(std::clamp(selectedRow, 0,
                                static_cast<int>(analysis.items.size()) - 1));
}

// Updates row-action availability and the explicit mode selector.
void RiderFixtureResolutionDialog::RefreshSelectionControls() {
  rider_fixture_resolution::Item *item = SelectedItem();
  useSuggestedButton->Enable(item && item->state == rider_fixture_resolution::State::Suggested &&
                             item->suggestedEntry.has_value());
  searchButton->Enable(item && item->state != rider_fixture_resolution::State::Dictionary);
  useGenericButton->Enable(item && item->state != rider_fixture_resolution::State::Dictionary);
  modeChoice->Clear();
  if (!item || !item->selectedEntry) {
    modeChoice->Enable(false);
    return;
  }
  for (const auto &mode : item->selectedEntry->modes)
    modeChoice->Append(wxString::FromUTF8(mode.name));
  modeChoice->Enable(item->selectedEntry->modes.size() > 1);
  if (!item->selectedMode.empty()) {
    const int index = modeChoice->FindString(wxString::FromUTF8(item->selectedMode));
    if (index != wxNOT_FOUND)
      modeChoice->SetSelection(index);
  }
}

// Updates completion counts and enables confirmation only for ready rows.
void RiderFixtureResolutionDialog::RefreshSummary() {
  const size_t ready = static_cast<size_t>(std::count_if(
      analysis.items.begin(), analysis.items.end(),
      [](const auto &item) { return item.IsReady(); }));
  summaryLabel->SetLabel(wxString::Format(_("%zu of %zu fixture types ready"),
                                          ready, analysis.items.size()));
  resolveButton->Enable(ready == analysis.items.size());
}

// Returns the model item corresponding to the selected table row.
rider_fixture_resolution::Item *RiderFixtureResolutionDialog::SelectedItem() {
  const int row = table->GetSelectedRow();
  if (row < 0 || row >= static_cast<int>(analysis.items.size()))
    return nullptr;
  return &analysis.items[static_cast<size_t>(row)];
}

// Refreshes actions when the selected row changes.
void RiderFixtureResolutionDialog::OnSelectionChanged(wxDataViewEvent &event) {
  RefreshSelectionControls();
  event.Skip();
}

// Accepts the safe suggestion for the selected row.
void RiderFixtureResolutionDialog::OnUseSuggested(wxCommandEvent &) {
  auto *item = SelectedItem();
  if (item && item->suggestedEntry)
    rider_fixture_resolution::Service::SelectCatalogEntry(*item, *item->suggestedEntry);
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
}

// Opens the established catalog browser pre-filtered for the rider alias.
void RiderFixtureResolutionDialog::OnSearch(wxCommandEvent &) {
  auto *item = SelectedItem();
  if (!item)
    return;
  if (catalogPayload.empty()) {
    const auto loaded = catalogLoader ? catalogLoader() : std::nullopt;
    if (!loaded) {
      wxMessageBox(_("The GDTF catalog is unavailable. You can use generic or cancel without changing the scene."),
                   _("GDTF catalog unavailable"), wxOK | wxICON_INFORMATION, this);
      return;
    }
    catalogPayload = loaded->listData;
    catalogUpdatedAt = loaded->updatedAt;
  }
  GdtfSearchDialog dialog(this, catalogPayload, catalogUpdatedAt, nullptr,
                          GdtfCatalogDisplaySource::Cached, true,
                          item->request.typeName);
  if (dialog.ShowModal() != wxID_OK)
    return;
  const auto selected = dialog.GetSelectedEntry();
  if (selected)
    rider_fixture_resolution::Service::SelectCatalogEntry(*item, *selected);
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
}

// Selects the existing one-import generic fallback for the selected alias.
void RiderFixtureResolutionDialog::OnUseGeneric(wxCommandEvent &) {
  if (auto *item = SelectedItem())
    rider_fixture_resolution::Service::SelectGeneric(*item);
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
}

// Accepts only safe Suggested rows and leaves Review rows untouched.
void RiderFixtureResolutionDialog::OnAcceptAll(wxCommandEvent &) {
  for (auto &item : analysis.items) {
    if (item.state == rider_fixture_resolution::State::Suggested &&
        item.suggestedEntry) {
      rider_fixture_resolution::Service::SelectCatalogEntry(item,
                                                             *item.suggestedEntry);
    }
  }
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
}

// Stores the user's explicit mode choice for a multi-mode profile.
void RiderFixtureResolutionDialog::OnModeSelected(wxCommandEvent &) {
  auto *item = SelectedItem();
  if (item && modeChoice->GetSelection() != wxNOT_FOUND)
    item->selectedMode = modeChoice->GetStringSelection().ToStdString();
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
}

// Closes successfully only after every non-dictionary row is explicitly ready.
void RiderFixtureResolutionDialog::OnResolve(wxCommandEvent &) {
  if (!std::all_of(analysis.items.begin(), analysis.items.end(),
                   [](const auto &item) { return item.IsReady(); })) {
    wxMessageBox(_("Resolve every fixture type or choose Use generic before continuing."),
                 _("Fixture types require attention"), wxOK | wxICON_WARNING,
                 this);
    return;
  }
  EndModal(wxID_OK);
}
