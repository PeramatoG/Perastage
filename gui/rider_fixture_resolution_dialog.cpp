#include "rider_fixture_resolution_dialog.h"

#include "gdtfsearchdialog.h"
#include "gdtfloader.h"
#include "../core/diagnostics/DiagnosticLogger.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>

#include <wx/button.h>
#include <wx/choicdlg.h>
#include <wx/dataview.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/thread.h>
#include <wx/weakref.h>

namespace {

const wxEventTypeTag<wxThreadEvent> EVT_RIDER_CATALOG_LOADED(wxNewEventType());

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
  if (item.state == rider_fixture_resolution::State::Dictionary &&
      item.dictionaryEntry && !item.dictionaryEntry->path.empty()) {
    return wxString::FromUTF8(
        std::filesystem::path(item.dictionaryEntry->path).filename().string());
  }
  if (item.state == rider_fixture_resolution::State::Generic ||
      (!item.selectedEntry && !item.suggestedEntry))
    return _("Generic fallback");
  if (!item.selectedEntry && item.suggestedEntry) {
    const auto &suggested = *item.suggestedEntry;
    return wxString::FromUTF8(
        "Generic fallback (suggested: " + suggested.manufacturer + " / " +
        suggested.fixtureName + ")");
  }
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
    std::unordered_map<std::string, GdtfDictionary::Entry> dictionaryIn,
    CatalogLoader cachedCatalogLoaderIn, CatalogLoader onlineCatalogLoaderIn)
    : wxDialog(parent, wxID_ANY, _("Resolve fixture types"), wxDefaultPosition,
               wxSize(1160, 680),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      analysis(std::move(analysisIn)),
      dictionary(std::move(dictionaryIn)),
      cachedCatalogLoader(std::move(cachedCatalogLoaderIn)),
      onlineCatalogLoader(std::move(onlineCatalogLoaderIn)) {
  BuildLayout();
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
  SetMinSize(wxSize(780, 500));
  CentreOnParent();
  Bind(wxEVT_SHOW, &RiderFixtureResolutionDialog::OnDialogShown, this);
  Bind(EVT_RIDER_CATALOG_LOADED,
       &RiderFixtureResolutionDialog::OnCatalogLoaded, this);
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
  catalogStatusLabel = new wxStaticText(
      this, wxID_ANY, _("Loading cached GDTF catalog..."));
  root->Add(catalogStatusLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxDV_ROW_LINES);
  const int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
  table->AppendToggleColumn(_("Create"), wxDATAVIEW_CELL_ACTIVATABLE, 65,
                            wxALIGN_CENTER, flags);
  table->AppendTextColumn(_("Fixture type"), wxDATAVIEW_CELL_EDITABLE, 220,
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
  actions->Add(useSuggestedButton, 0, wxRIGHT, 8);
  actions->Add(searchButton, 0, wxRIGHT, 8);
  actions->Add(useGenericButton, 0);
  actions->AddStretchSpacer(1);
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
  table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
              &RiderFixtureResolutionDialog::OnItemActivated, this);
  table->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED,
              &RiderFixtureResolutionDialog::OnValueChanged, this);
  useSuggestedButton->Bind(wxEVT_BUTTON,
                           &RiderFixtureResolutionDialog::OnUseSuggested, this);
  searchButton->Bind(wxEVT_BUTTON, &RiderFixtureResolutionDialog::OnSearch,
                     this);
  useGenericButton->Bind(wxEVT_BUTTON,
                         &RiderFixtureResolutionDialog::OnUseGeneric, this);
  acceptAllButton->Bind(wxEVT_BUTTON,
                        &RiderFixtureResolutionDialog::OnAcceptAll, this);
  resolveButton->Bind(wxEVT_BUTTON, &RiderFixtureResolutionDialog::OnResolve,
                      this);
}

// Rebuilds table rows from the current resolution model.
void RiderFixtureResolutionDialog::RefreshTable() {
  const int selectedRow = table->GetSelectedRow();
  table->DeleteAllItems();
  for (const auto &item : analysis.items) {
    wxVector<wxVariant> row;
    row.push_back(item.create);
    row.push_back(wxString::FromUTF8(item.effectiveFixtureType));
    row.push_back(wxString::Format("%d", item.request.quantity));
    row.push_back(JoinPositions(item.request.positions));
    row.push_back(FormatCatalogIdentity(item));
    row.push_back(item.selectedMode.empty()
                      ? wxString("-")
                      : wxString::FromUTF8(item.selectedMode));
    row.push_back(wxString::FromUTF8(rider_fixture_resolution::OriginName(item.origin)));
    row.push_back(wxString::FromUTF8(item.details));
    table->AppendItem(row);
  }
  if (!analysis.items.empty())
    table->SelectRow(std::clamp(selectedRow, 0,
                                static_cast<int>(analysis.items.size()) - 1));
}

// Updates row-action availability for the selected fixture row.
void RiderFixtureResolutionDialog::RefreshSelectionControls() {
  rider_fixture_resolution::Item *item = SelectedItem();
  useSuggestedButton->Enable(item && item->create && item->state == rider_fixture_resolution::State::Suggested &&
                             item->suggestedEntry.has_value());
  searchButton->Enable(item && item->create && item->state != rider_fixture_resolution::State::Dictionary);
  useGenericButton->Enable(item && item->create && item->state != rider_fixture_resolution::State::Dictionary);
}

// Updates the real-mapping count while keeping Generic confirmation available.
void RiderFixtureResolutionDialog::RefreshSummary() {
  const size_t created = static_cast<size_t>(std::count_if(
      analysis.items.begin(), analysis.items.end(), [](const auto &item) {
        return item.create;
      }));
  const size_t automatic = static_cast<size_t>(std::count_if(
      analysis.items.begin(), analysis.items.end(),
      [](const auto &item) {
        return item.create && item.origin == rider_fixture_resolution::ResolutionOrigin::AutomaticMatch;
      }));
  const size_t generic = static_cast<size_t>(std::count_if(
      analysis.items.begin(), analysis.items.end(), [](const auto &item) {
        return item.create && item.origin == rider_fixture_resolution::ResolutionOrigin::GenericFallback;
      }));
  const size_t skipped = analysis.items.size() - created;
  summaryLabel->SetLabel(wxString::Format(
      _("%zu fixture types will be created · %zu automatic matches · %zu generic · %zu skipped"),
      created, automatic, generic, skipped));
  resolveButton->Enable(true);
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

// Edits a row mode through the activated Mode cell using valid profile modes.
void RiderFixtureResolutionDialog::OnItemActivated(wxDataViewEvent &event) {
  if (event.GetColumn() != 5)
    return;
  const int activatedRow = table->ItemToRow(event.GetItem());
  if (activatedRow != wxNOT_FOUND)
    table->SelectRow(activatedRow);
  auto *item = SelectedItem();
  if (!item)
    return;
  if (!item->create)
    return;
  std::vector<std::string> modes;
  if (item->state == rider_fixture_resolution::State::Dictionary &&
      item->dictionaryEntry) {
    modes = GetGdtfModes(item->dictionaryEntry->path);
  } else if (item->selectedEntry) {
    for (const auto &mode : item->selectedEntry->modes) {
      if (!mode.name.empty())
        modes.push_back(mode.name);
    }
  }
  if (modes.empty())
    return;
  if (modes.size() == 1) {
    item->selectedMode = modes.front();
  } else {
    wxArrayString choices;
    for (const std::string &mode : modes)
      choices.Add(wxString::FromUTF8(mode));
    wxSingleChoiceDialog choice(
        this, _("Choose a valid mode for this fixture row."),
        _("Fixture mode"), choices);
    const int current = choices.Index(wxString::FromUTF8(item->selectedMode));
    if (current != wxNOT_FOUND)
      choice.SetSelection(current);
    if (choice.ShowModal() != wxID_OK)
      return;
    item->selectedMode = choice.GetStringSelection().ToStdString();
  }
  if (item->dictionaryEntry) {
    item->origin = item->selectedMode == item->originalDictionaryMode
        ? rider_fixture_resolution::ResolutionOrigin::Dictionary
        : rider_fixture_resolution::ResolutionOrigin::DictionaryModified;
  }
  RefreshTable();
  RefreshSummary();
}

// Applies committed Create and Fixture type cell edits to the resolution model.
void RiderFixtureResolutionDialog::OnValueChanged(wxDataViewEvent &event) {
  const int row = table->ItemToRow(event.GetItem());
  if (row == wxNOT_FOUND || row >= static_cast<int>(analysis.items.size()))
    return;
  auto &item = analysis.items[static_cast<size_t>(row)];
  wxVariant value;
  table->GetValue(value, static_cast<unsigned int>(row),
                  static_cast<unsigned int>(event.GetColumn()));
  if (event.GetColumn() == 0) {
    rider_fixture_resolution::Service::SetCreate(item, value.GetBool());
    if (item.create)
      rider_fixture_resolution::Service::ResolveItem(item, dictionary,
                                                      catalogEntries);
  } else if (event.GetColumn() == 1) {
    const std::string edited =
        mvr::gdtf_catalog_matcher::TrimFixtureIdentity(value.GetString().ToStdString());
    item.effectiveFixtureType = edited.empty() ? item.originalFixtureType : edited;
    rider_fixture_resolution::Service::ResolveItem(item, dictionary,
                                                    catalogEntries);
  }
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
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
  if (catalogEntries.empty()) {
    if (catalogLoading) {
      wxMessageBox(_("The cached catalog is still loading. Generic fallback remains available."),
                   _("GDTF catalog"), wxOK | wxICON_INFORMATION, this);
      return;
    }
    const auto loaded = onlineCatalogLoader ? onlineCatalogLoader() : std::nullopt;
    if (!loaded) {
      wxMessageBox(_("The GDTF catalog is unavailable. You can use generic or cancel without changing the scene."),
                   _("GDTF catalog unavailable"), wxOK | wxICON_INFORMATION, this);
      return;
    }
    ApplyCatalog(*loaded);
  }
  GdtfSearchDialog dialog(this, catalogPayload, catalogUpdatedAt, nullptr,
                          catalogSource == CatalogSource::Online
                              ? GdtfCatalogDisplaySource::Online
                              : GdtfCatalogDisplaySource::Cached,
                          catalogSource != CatalogSource::Online,
                          item->effectiveFixtureType, catalogEntries);
  if (dialog.ShowModal() != wxID_OK)
    return;
  const auto selected = dialog.GetSelectedEntry();
  if (selected)
    rider_fixture_resolution::Service::SelectCatalogEntry(
        *item, *selected,
        rider_fixture_resolution::ResolutionOrigin::UserSelection);
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
}

// Starts cached catalog parsing only after the modal dialog becomes visible.
void RiderFixtureResolutionDialog::OnDialogShown(wxShowEvent &event) {
  event.Skip();
  if (!event.IsShown() || catalogLoadStarted || !cachedCatalogLoader)
    return;
  catalogLoadStarted = true;
  catalogLoading = true;
  const CatalogLoader loader = cachedCatalogLoader;
  const auto requests = BuildFixtureRequests();
  wxWeakRef<RiderFixtureResolutionDialog> weakDialog(this);
  std::thread([loader, weakDialog, requests]() mutable {
    auto loaded = loader();
    if (loaded) {
      const auto matchStarted = std::chrono::steady_clock::now();
      loaded->matches = rider_fixture_resolution::Service::Analyze(
          requests, {}, loaded->entries);
      loaded->matchMs =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - matchStarted).count();
    }
    RiderFixtureResolutionDialog *dialog = weakDialog.get();
    if (!dialog)
      return;
    auto *event = new wxThreadEvent(EVT_RIDER_CATALOG_LOADED);
    event->SetPayload(loaded);
    wxQueueEvent(dialog, event);
  }).detach();
}

// Applies a completed cached catalog load on the wxWidgets UI thread.
void RiderFixtureResolutionDialog::OnCatalogLoaded(wxThreadEvent &event) {
  catalogLoading = false;
  const auto loaded =
      event.GetPayload<std::optional<RiderFixtureResolutionDialog::CatalogData>>();
  if (!loaded) {
    catalogStatusLabel->SetLabel(
        _("Catalog unavailable; generic fallback remains available."));
    return;
  }
  ApplyCatalog(*loaded);
}

// Merges catalog suggestions into untouched rows and refreshes presentation.
void RiderFixtureResolutionDialog::ApplyCatalog(const CatalogData &catalog) {
  catalogPayload = catalog.snapshot.listData;
  catalogUpdatedAt = catalog.snapshot.updatedAt;
  catalogEntries = catalog.entries;
  catalogSource = catalog.source;
  const auto matched = catalog.matches
      ? *catalog.matches
      : rider_fixture_resolution::Service::Analyze(BuildFixtureRequests(), {},
                                                    catalogEntries);
  diagnostics::DiagnosticLogger::Info(
      "Rider fixture preflight catalog: catalog_match_ms=" +
      std::to_string(catalog.matchMs));
  rider_fixture_resolution::Service::MergeCatalogSuggestions(analysis, matched);
  catalogStatusLabel->SetLabel(wxString::Format(
      _("Catalog loaded: %zu entries"), catalogEntries.size()));
  RefreshTable();
  RefreshSelectionControls();
  RefreshSummary();
}

// Copies fixture requests for pure background catalog matching.
std::vector<RiderImporter::FixtureTypeRequest>
RiderFixtureResolutionDialog::BuildFixtureRequests() const {
  std::vector<RiderImporter::FixtureTypeRequest> requests;
  requests.reserve(analysis.items.size());
  for (const auto &item : analysis.items)
  {
    auto request = item.request;
    request.typeName = item.effectiveFixtureType;
    request.normalizedTypeName = GdtfDictionary::NormalizeTypeKey(
        item.effectiveFixtureType);
    requests.push_back(std::move(request));
  }
  return requests;
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

// Closes successfully only after every non-dictionary row is explicitly ready.
void RiderFixtureResolutionDialog::OnResolve(wxCommandEvent &) {
  rider_fixture_resolution::Service::FinalizeDefaults(analysis);
  EndModal(wxID_OK);
}
