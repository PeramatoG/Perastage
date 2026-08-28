#include "rider_fixture_resolution_dialog.h"

#include "gdtfsearchdialog.h"
#include "gdtfloader.h"
#include "rider_fixture_resolution_model.h"
#include "../core/diagnostics/DiagnosticLogger.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <sstream>

#include <wx/button.h>
#include <wx/choicdlg.h>
#include <wx/dataview.h>
#include <wx/gauge.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/thread.h>

namespace {

const wxEventTypeTag<wxThreadEvent> EVT_RIDER_CATALOG_LOADED(wxNewEventType());
const wxEventTypeTag<wxThreadEvent> EVT_RIDER_ONLINE_CATALOG_LOADED(
    wxNewEventType());
const wxEventTypeTag<wxThreadEvent> EVT_RIDER_CATALOG_PROGRESS(wxNewEventType());

} // namespace

// Creates the modal fixture-resolution review without starting downloads.
RiderFixtureResolutionDialog::RiderFixtureResolutionDialog(
    wxWindow *parent, rider_fixture_resolution::Analysis analysisIn,
    std::unordered_map<std::string, GdtfDictionary::Entry> dictionaryIn,
    CatalogLoader cachedCatalogLoaderIn,
    OnlineCatalogLoader onlineCatalogLoaderIn,
    std::optional<CredentialStore::Credentials> initialCredentials,
    CredentialRequester credentialRequesterIn,
    CredentialPersistCallback credentialPersistCallbackIn)
    : wxDialog(parent, wxID_ANY, _("Resolve fixture types"), wxDefaultPosition,
               wxSize(1160, 680),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      analysis(std::move(analysisIn)),
      dictionary(std::move(dictionaryIn)),
      cachedCatalogLoader(std::move(cachedCatalogLoaderIn)),
      onlineCatalogLoader(std::move(onlineCatalogLoaderIn)),
      catalogCredentials(std::move(initialCredentials)),
      credentialRequester(std::move(credentialRequesterIn)),
      credentialPersistCallback(std::move(credentialPersistCallbackIn)) {
  BuildLayout();
  PopulateTable();
  RefreshSelectionControls();
  RefreshSummary();
  SetMinSize(wxSize(780, 500));
  CentreOnParent();
  Bind(wxEVT_SHOW, &RiderFixtureResolutionDialog::OnDialogShown, this);
  Bind(EVT_RIDER_CATALOG_LOADED,
       &RiderFixtureResolutionDialog::OnCatalogLoaded, this);
  Bind(EVT_RIDER_ONLINE_CATALOG_LOADED,
       &RiderFixtureResolutionDialog::OnOnlineCatalogLoaded, this);
  Bind(EVT_RIDER_CATALOG_PROGRESS,
       &RiderFixtureResolutionDialog::OnProgress, this);
}

// Stops the owned catalog worker before wxWidgets destroys the event handler.
RiderFixtureResolutionDialog::~RiderFixtureResolutionDialog() {
  shuttingDown.store(true);
  RequestWorkerStop();
  catalogWorker.Join();
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
      this, wxID_ANY, _("Checking cached GDTF catalog..."));
  root->Add(catalogStatusLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
  catalogProgressGauge = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition,
                                     wxSize(-1, 6), wxGA_HORIZONTAL | wxGA_SMOOTH);
  catalogProgressGauge->SetValue(0);
  root->Add(catalogProgressGauge, 0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  table = new wxDataViewCtrl(this, wxID_ANY, wxDefaultPosition,
                             wxDefaultSize, wxDV_ROW_LINES);
  tableModel = new RiderFixtureResolutionModel(analysis);
  wxASSERT(tableModel->GetColumnCount() ==
           RiderFixtureResolutionModel::ColumnCount);
  table->AssociateModel(tableModel);
  tableModel->DecRef();
  const int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
  table->AppendToggleColumn(_("Create"), 0, wxDATAVIEW_CELL_ACTIVATABLE, 65,
                            wxALIGN_CENTER, flags);
  table->AppendTextColumn(_("Fixture type"), 1, wxDATAVIEW_CELL_EDITABLE, 220,
                          wxALIGN_LEFT, flags);
  table->AppendTextColumn(_("Qty"), 2, wxDATAVIEW_CELL_INERT, 55, wxALIGN_RIGHT,
                          flags);
  table->AppendTextColumn(_("Positions"), 3, wxDATAVIEW_CELL_INERT, 140,
                          wxALIGN_LEFT, flags);
  table->AppendTextColumn(_("Selected GDTF"), 4, wxDATAVIEW_CELL_INERT, 270,
                          wxALIGN_LEFT, flags);
  table->AppendTextColumn(_("Mode"), 5, wxDATAVIEW_CELL_INERT, 130, wxALIGN_LEFT,
                          flags);
  table->AppendTextColumn(_("Status"), 6, wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT,
                          flags);
  table->AppendTextColumn(_("Details"), 7, wxDATAVIEW_CELL_INERT, 220,
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
  acceptAllButton = new wxButton(this, wxID_ANY, _("Accept all suggestions"));
  acceptAllButton->Enable(false);
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
  cancelButton->Bind(wxEVT_BUTTON, &RiderFixtureResolutionDialog::OnCancel,
                     this);
}

// Validates the fixed table schema and selects the initial analysis row.
void RiderFixtureResolutionDialog::PopulateTable() {
  wxASSERT(tableModel->GetColumnCount() ==
           RiderFixtureResolutionModel::ColumnCount);
  if (!analysis.items.empty())
    table->Select(tableModel->GetItem(0));
}

// Updates one stable model row and its semantic Status attribute in place.
void RiderFixtureResolutionDialog::UpdateRow(size_t analysisIndex) {
  if (analysisIndex >= analysis.items.size())
    return;
  modelUpdateInProgress = true;
  tableModel->NotifyRowChanged(analysisIndex);
  modelUpdateInProgress = false;
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
  const auto summary = rider_fixture_resolution::Service::Summarize(analysis);
  summaryLabel->SetLabel(wxString::Format(
      _("%zu fixture types will be created | %zu automatic matches | %zu generic | %zu skipped"),
      summary.created, summary.automaticMatches, summary.genericFallbacks,
      summary.skipped));
  resolveButton->Enable(true);
}

// Shows a stable completion message derived from the applied resolution plan.
void RiderFixtureResolutionDialog::RefreshCatalogCompletionStatus() {
  const auto summary = rider_fixture_resolution::Service::Summarize(analysis);
  if (summary.automaticMatches == 0) {
    catalogStatusLabel->SetLabel(
        _("Automatic matching complete | No additional matches found"));
    return;
  }
  catalogStatusLabel->SetLabel(wxString::Format(
      _("Automatic matching complete | %zu matches | %zu generic fallbacks"),
      summary.automaticMatches, summary.genericFallbacks));
}

// Returns the model item corresponding to the selected table row.
rider_fixture_resolution::Item *RiderFixtureResolutionDialog::SelectedItem() {
  const auto index = AnalysisIndexForItem(table->GetSelection());
  if (!index || *index >= analysis.items.size())
    return nullptr;
  return &analysis.items[*index];
}

// Resolves a stable index-list model item to its analysis row.
std::optional<size_t> RiderFixtureResolutionDialog::AnalysisIndexForItem(
    const wxDataViewItem &item) const {
  if (!item.IsOk())
    return std::nullopt;
  const size_t index = static_cast<size_t>(tableModel->GetRow(item));
  return index < analysis.items.size() ? std::optional<size_t>(index)
                                       : std::nullopt;
}

// Maps stable analysis identity directly to its fixed model row.
std::optional<unsigned>
RiderFixtureResolutionDialog::StoreRowForAnalysisIndex(size_t analysisIndex) const {
  return analysisIndex < analysis.items.size()
             ? std::optional<unsigned>(static_cast<unsigned>(analysisIndex))
             : std::nullopt;
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
  const auto analysisIndex = AnalysisIndexForItem(event.GetItem());
  if (!analysisIndex)
    return;
  table->Select(event.GetItem());
  auto &resolvedItem = analysis.items[*analysisIndex];
  auto *item = &resolvedItem;
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
  UpdateRow(*analysisIndex);
  RefreshSummary();
}

// Applies committed Create and Fixture type cell edits to the resolution model.
void RiderFixtureResolutionDialog::OnValueChanged(wxDataViewEvent &event) {
  if (modelUpdateInProgress)
    return;
  const auto analysisIndex = AnalysisIndexForItem(event.GetItem());
  if (!analysisIndex)
    return;
  const auto storeRow = StoreRowForAnalysisIndex(*analysisIndex);
  if (!storeRow || *storeRow >= analysis.items.size())
    return;
  auto &item = analysis.items[*analysisIndex];
  wxVariant value;
  const int eventColumn = event.GetColumn();
  if (eventColumn < 0 ||
      static_cast<unsigned>(eventColumn) >= tableModel->GetColumnCount())
    return;
  tableModel->GetValueByRow(value, *storeRow,
                            static_cast<unsigned int>(eventColumn));
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
  UpdateRow(*analysisIndex);
  RefreshSelectionControls();
  RefreshSummary();
}

// Accepts the safe suggestion for the selected row.
void RiderFixtureResolutionDialog::OnUseSuggested(wxCommandEvent &) {
  const auto analysisIndex = AnalysisIndexForItem(table->GetSelection());
  auto *item = analysisIndex ? &analysis.items[*analysisIndex] : nullptr;
  if (item && item->suggestedEntry)
    rider_fixture_resolution::Service::SelectCatalogEntry(*item, *item->suggestedEntry);
  if (analysisIndex)
    UpdateRow(*analysisIndex);
  RefreshSelectionControls();
  RefreshSummary();
}

// Opens the established catalog browser pre-filtered for the rider alias.
void RiderFixtureResolutionDialog::OnSearch(wxCommandEvent &) {
  const auto analysisIndex = AnalysisIndexForItem(table->GetSelection());
  auto *item = analysisIndex ? &analysis.items[*analysisIndex] : nullptr;
  if (!item)
    return;
  if (catalogEntries.empty()) {
    if (catalogLoading) {
      wxMessageBox(_("The cached catalog is still loading. Generic fallback remains available."),
                   _("GDTF catalog"), wxOK | wxICON_INFORMATION, this);
      return;
    }
    if (onlineCatalogLoader) {
      onlineCatalogLoadAttempted = true;
      BeginOnlineCatalogAcquisition();
    } else {
      wxMessageBox(_("The GDTF catalog is unavailable. You can use generic or cancel without changing the scene."),
                   _("GDTF catalog unavailable"), wxOK | wxICON_INFORMATION, this);
    }
    return;
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
  UpdateRow(*analysisIndex);
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
  const auto targets = BuildCatalogMatchTargets();
  const size_t analysisItemCount = analysis.items.size();
  catalogWorker.Start(
      [this, loader, targets,
       analysisItemCount](RiderFixtureResolutionStopToken stopToken) mutable {
    auto report = [this, &stopToken](const ProgressData &progress) {
      if (stopToken.stop_requested() || shuttingDown.load())
        return;
      auto *event = new wxThreadEvent(EVT_RIDER_CATALOG_PROGRESS);
      event->SetPayload(progress);
      wxQueueEvent(this, event);
    };
    report({{rider_fixture_resolution::ProgressStage::LoadingCatalog}});
    report({{rider_fixture_resolution::ProgressStage::ParsingCatalog}});
    auto loaded = loader();
    if (loaded) {
      const auto matchStarted = std::chrono::steady_clock::now();
      rider_fixture_resolution::Analysis matches;
      matches.items.resize(analysisItemCount);
      size_t automaticMatches = 0;
      for (size_t targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
        if (stopToken.stop_requested() || shuttingDown.load())
          return;
        const auto &target = targets[targetIndex];
        auto one = rider_fixture_resolution::Service::Analyze(
            {target.request}, {}, loaded->entries);
        if (one.items.front().origin ==
            rider_fixture_resolution::ResolutionOrigin::AutomaticMatch)
          ++automaticMatches;
        matches.items[target.analysisIndex] = one.items.front();
        report({{rider_fixture_resolution::ProgressStage::MatchingFixtures,
                 targetIndex + 1, targets.size(), automaticMatches},
                target.analysisIndex, one.items.front()});
      }
      report({{rider_fixture_resolution::ProgressStage::Complete,
               targets.size(), targets.size(), automaticMatches}});
      loaded->matches = std::move(matches);
      loaded->matchMs =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - matchStarted).count();
    } else {
      report({{rider_fixture_resolution::ProgressStage::Unavailable}});
    }
    if (stopToken.stop_requested() || shuttingDown.load())
      return;
    auto *event = new wxThreadEvent(EVT_RIDER_CATALOG_LOADED);
    event->SetPayload(loaded);
    wxQueueEvent(this, event);
  });
}

// Applies a completed cached catalog load on the wxWidgets UI thread.
void RiderFixtureResolutionDialog::OnCatalogLoaded(wxThreadEvent &event) {
  catalogLoading = false;
  if (!acceptAutomaticResults)
    return;
  const auto loaded =
      event.GetPayload<std::optional<RiderFixtureResolutionDialog::CatalogData>>();
  if (!loaded) {
    if (onlineCatalogLoader && !onlineCatalogLoadAttempted) {
      onlineCatalogLoadAttempted = true;
      BeginOnlineCatalogAcquisition();
      return;
    }
    catalogStatusLabel->SetLabel(
        _("Catalog unavailable; generic fallback remains available."));
    catalogProgressGauge->SetValue(0);
    return;
  }
  ApplyCatalog(*loaded);
}

// Requests credentials on the UI thread before starting online acquisition.
void RiderFixtureResolutionDialog::BeginOnlineCatalogAcquisition(
    bool rejectedCredentials) {
  if (!acceptAutomaticResults || shuttingDown.load())
    return;
  if (rejectedCredentials)
    catalogCredentials.reset();
  if (!catalogCredentials) {
    catalogStatusLabel->SetLabel(
        _("Waiting for GDTF Share credentials..."));
    catalogProgressGauge->Pulse();
    catalogCredentials = credentialRequester
                             ? credentialRequester(rejectedCredentials)
                             : std::nullopt;
    if (!catalogCredentials) {
      catalogStatusLabel->SetLabel(
          _("GDTF catalog sign-in cancelled - generic fallback remains available"));
      catalogProgressGauge->SetValue(0);
      return;
    }
  }
  catalogStatusLabel->SetLabel(_("Connecting to GDTF Share..."));
  catalogProgressGauge->Pulse();
  StartOnlineCatalogWorker(*catalogCredentials);
}

// Starts owned background authentication, refresh, parsing, and matching work.
void RiderFixtureResolutionDialog::StartOnlineCatalogWorker(
    const CredentialStore::Credentials &credentials) {
  RequestWorkerStop();
  catalogWorker.Join();
  catalogLoading = true;
  const auto targets = BuildCatalogMatchTargets();
  const size_t analysisItemCount = analysis.items.size();
  const OnlineCatalogLoader loader = onlineCatalogLoader;
  catalogWorker.Start(
      [this, loader, credentials, targets,
       analysisItemCount](RiderFixtureResolutionStopToken stopToken) mutable {
    auto report = [this, &stopToken](const ProgressData &progress) {
      if (stopToken.stop_requested() || shuttingDown.load())
        return;
      auto *event = new wxThreadEvent(EVT_RIDER_CATALOG_PROGRESS);
      event->SetPayload(progress);
      wxQueueEvent(this, event);
    };
    OnlineCatalogResult result = loader(
        credentials, stopToken,
        [&](const rider_fixture_resolution::Progress &progress) {
          report({progress});
        });
    if (result.catalog && !stopToken.stop_requested() &&
        !shuttingDown.load()) {
      auto &catalog = *result.catalog;
      const auto matchStarted = std::chrono::steady_clock::now();
      rider_fixture_resolution::Analysis matches;
      matches.items.resize(analysisItemCount);
      size_t automaticMatches = 0;
      for (size_t targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
        if (stopToken.stop_requested() || shuttingDown.load())
          return;
        const auto &target = targets[targetIndex];
        auto one = rider_fixture_resolution::Service::Analyze(
            {target.request}, {}, catalog.entries);
        if (one.items.front().origin ==
            rider_fixture_resolution::ResolutionOrigin::AutomaticMatch)
          ++automaticMatches;
        matches.items[target.analysisIndex] = one.items.front();
        report({{rider_fixture_resolution::ProgressStage::MatchingFixtures,
                 targetIndex + 1, targets.size(), automaticMatches},
                target.analysisIndex, one.items.front()});
      }
      report({{rider_fixture_resolution::ProgressStage::Complete,
               targets.size(), targets.size(), automaticMatches}});
      catalog.matches = std::move(matches);
      catalog.matchMs =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - matchStarted).count();
    }
    if (stopToken.stop_requested() || shuttingDown.load())
      return;
    auto *event = new wxThreadEvent(EVT_RIDER_ONLINE_CATALOG_LOADED);
    event->SetPayload(result);
    wxQueueEvent(this, event);
  });
}

// Applies online acquisition results or returns to credential entry/fallback.
void RiderFixtureResolutionDialog::OnOnlineCatalogLoaded(wxThreadEvent &event) {
  catalogLoading = false;
  if (!acceptAutomaticResults)
    return;
  const auto result = event.GetPayload<OnlineCatalogResult>();
  if (result.status == OnlineCatalogStatus::AuthenticationRejected) {
    BeginOnlineCatalogAcquisition(true);
    return;
  }
  if (result.status != OnlineCatalogStatus::Success || !result.catalog) {
    diagnostics::DiagnosticLogger::Warning(
        "Rider fixture catalog acquisition unavailable: " + result.error);
    catalogStatusLabel->SetLabel(
        _("GDTF catalog unavailable - generic fallback remains available"));
    catalogProgressGauge->SetValue(0);
    return;
  }
  if (catalogCredentials && credentialPersistCallback)
    credentialPersistCallback(*catalogCredentials);
  ApplyCatalog(*result.catalog);
  catalogProgressGauge->SetRange(
      static_cast<int>(std::max<size_t>(analysis.items.size(), 1)));
  catalogProgressGauge->SetValue(static_cast<int>(analysis.items.size()));
  RefreshCatalogCompletionStatus();
}

// Updates the visible gauge from real catalog and matching worker progress.
void RiderFixtureResolutionDialog::OnProgress(wxThreadEvent &event) {
  if (!acceptAutomaticResults)
    return;
  const auto data = event.GetPayload<ProgressData>();
  const auto &progress = data.progress;
  if (data.matchedItem && data.row < analysis.items.size()) {
    rider_fixture_resolution::Service::MergeCatalogSuggestion(
        analysis.items[data.row], *data.matchedItem);
    UpdateRow(data.row);
    RefreshSelectionControls();
    RefreshSummary();
  }
  switch (progress.stage) {
  case rider_fixture_resolution::ProgressStage::LoadingCatalog:
    catalogStatusLabel->SetLabel(_("Checking cached GDTF catalog..."));
    catalogProgressGauge->Pulse();
    break;
  case rider_fixture_resolution::ProgressStage::WaitingForCredentials:
    catalogStatusLabel->SetLabel(_("Waiting for GDTF Share credentials..."));
    catalogProgressGauge->Pulse();
    break;
  case rider_fixture_resolution::ProgressStage::Authenticating:
    catalogStatusLabel->SetLabel(_("Signing in to GDTF Share..."));
    catalogProgressGauge->Pulse();
    break;
  case rider_fixture_resolution::ProgressStage::DownloadingCatalog:
    catalogStatusLabel->SetLabel(_("Downloading GDTF catalog..."));
    catalogProgressGauge->Pulse();
    break;
  case rider_fixture_resolution::ProgressStage::ParsingCatalog:
    catalogStatusLabel->SetLabel(_("Parsing GDTF catalog..."));
    catalogProgressGauge->Pulse();
    break;
  case rider_fixture_resolution::ProgressStage::MatchingFixtures:
    catalogProgressGauge->SetRange(static_cast<int>(std::max<size_t>(progress.total, 1)));
    catalogProgressGauge->SetValue(static_cast<int>(progress.current));
    catalogStatusLabel->SetLabel(wxString::Format(
        _("Matching GDTF candidates... %zu / %zu"), progress.current,
        progress.total));
    break;
  case rider_fixture_resolution::ProgressStage::Complete: {
    catalogProgressGauge->SetRange(static_cast<int>(std::max<size_t>(progress.total, 1)));
    catalogProgressGauge->SetValue(static_cast<int>(progress.total));
    RefreshCatalogCompletionStatus();
    acceptAllButton->Enable(true);
    break;
  }
  case rider_fixture_resolution::ProgressStage::Unavailable:
    catalogProgressGauge->SetValue(0);
    catalogStatusLabel->SetLabel(
        _("GDTF catalog unavailable - generic fallback remains available"));
    break;
  }
}

// Merges catalog suggestions into untouched rows and refreshes presentation.
void RiderFixtureResolutionDialog::ApplyCatalog(const CatalogData &catalog) {
  if (!acceptAutomaticResults)
    return;
  catalogPayload = catalog.snapshot.listData;
  catalogUpdatedAt = catalog.snapshot.updatedAt;
  catalogEntries = catalog.entries;
  catalogSource = catalog.source;
  rider_fixture_resolution::Analysis matched;
  if (catalog.matches) {
    matched = *catalog.matches;
  } else {
    matched.items.resize(analysis.items.size());
    for (const auto &target : BuildCatalogMatchTargets()) {
      auto one = rider_fixture_resolution::Service::Analyze(
          {target.request}, {}, catalogEntries);
      matched.items[target.analysisIndex] = std::move(one.items.front());
    }
  }
  diagnostics::DiagnosticLogger::Info(
      "Rider fixture preflight catalog: catalog_match_ms=" +
      std::to_string(catalog.matchMs));
  rider_fixture_resolution::Service::MergeCatalogSuggestions(analysis, matched);
  for (size_t index = 0; index < analysis.items.size(); ++index)
    UpdateRow(index);
  RefreshSelectionControls();
  RefreshSummary();
}

// Selects stable analysis rows that still need pure catalog matching.
std::vector<rider_fixture_resolution::CatalogMatchTarget>
RiderFixtureResolutionDialog::BuildCatalogMatchTargets() const {
  return rider_fixture_resolution::Service::BuildCatalogMatchTargets(analysis);
}

// Selects the existing one-import generic fallback for the selected alias.
void RiderFixtureResolutionDialog::OnUseGeneric(wxCommandEvent &) {
  const auto analysisIndex = AnalysisIndexForItem(table->GetSelection());
  if (auto *item = analysisIndex ? &analysis.items[*analysisIndex] : nullptr)
    rider_fixture_resolution::Service::SelectGeneric(*item);
  if (analysisIndex)
    UpdateRow(*analysisIndex);
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
  for (size_t index = 0; index < analysis.items.size(); ++index)
    UpdateRow(index);
  RefreshSelectionControls();
  RefreshSummary();
}

// Closes successfully only after every non-dictionary row is explicitly ready.
void RiderFixtureResolutionDialog::OnResolve(wxCommandEvent &) {
  acceptAutomaticResults = false;
  RequestWorkerStop();
  rider_fixture_resolution::Service::FinalizeDefaults(analysis);
  for (size_t index = 0; index < analysis.items.size(); ++index)
    UpdateRow(index);
  EndModal(wxID_OK);
}

// Cancels the modal workflow and requests cooperative worker shutdown.
void RiderFixtureResolutionDialog::OnCancel(wxCommandEvent &) {
  acceptAutomaticResults = false;
  RequestWorkerStop();
  EndModal(wxID_CANCEL);
}

// Requests cooperative cancellation without blocking the modal UI thread.
void RiderFixtureResolutionDialog::RequestWorkerStop() {
  catalogWorker.RequestStop();
}
