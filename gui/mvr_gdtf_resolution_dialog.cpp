/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "mvr_gdtf_resolution_dialog.h"
#include "../mvr/gdtf_import_matching.h"
#include "gdtf_resolution_status_style.h"
#include <wx/wx.h>
#include "gdtf_download_queue_table.h"
#include <wx/choice.h>
#include <algorithm>

wxDEFINE_EVENT(EVT_MVR_GDTF_DONE, wxThreadEvent);
wxDEFINE_EVENT(EVT_MVR_GDTF_PROGRESS, wxThreadEvent);
wxDEFINE_EVENT(EVT_MVR_GDTF_ANALYSIS, wxThreadEvent);
using namespace gdtf_resolution;
namespace {
wxString SourceText(Source source) {
  switch (source) {
  case Source::OriginalMvr: return _("Original MVR profile selected");
  case Source::Application: return _("Application fallback selected");
  case Source::Local: return _("Local profile selected");
  case Source::Catalog: return _("GDTF Share");
  default: return _("Unresolved");
  }
}
wxString StatusText(Status status) {
  switch (status) {
  case Status::Pending: return _("Pending");
  case Status::Downloading: return _("Downloading");
  case Status::Downloaded: return _("Downloaded successfully");
  case Status::ModeRequired: return _("Downloaded; select DMX mode");
  case Status::Identical: return _("Downloaded; identical to original MVR");
  case Status::Reused: return _("Reused existing profile");
  case Status::OriginalMvr: return SourceText(Source::OriginalMvr);
  case Status::Application: return SourceText(Source::Application);
  case Status::Local: return SourceText(Source::Local);
  case Status::DownloadFailed: return _("Download failed");
  case Status::Canceled: return _("Canceled");
  default: return _("Unresolved");
  }
}
} // namespace

MvrGdtfResolutionDialog::MvrGdtfResolutionDialog(wxWindow *parent,
    const std::vector<mvr::SceneReadGdtfConflict> &requests,
    const std::filesystem::path &root, MvrGdtfResolutionCallbacks services,
    bool automaticClose,
    const std::unordered_map<std::string, std::vector<std::string>> &importedModes)
    : GdtfDownloadQueueDialog(parent, false),
      sceneRoot(root), callbacks(std::move(services)), automaticClose(automaticClose) {
  for (const auto &request : requests) {
    Row row;
    row.request = request;
    row.evidence = mvr::gdtf_import_matching::BuildDownloadRequest(
        {request.type, request.fixtureName, request.requestedFixtureName,
         request.manufacturer, request.fixtureTypeId, request.modeName,
         request.footprint});
    if (const auto modes = importedModes.find(request.type); modes != importedModes.end())
      row.evidence.importedModes = modes->second;
    rows.push_back(std::move(row));
  }
  BuildLayout();
  activityTimer.SetOwner(this);
  Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
    if (!busy)
      return;
    if (!determinateProgress)
      gauge->Pulse();
    phase->SetLabel(wxString::Format(_("%s (%ld s)"), workPhase, phaseClock.Time() / 1000));
  }, activityTimer.GetId());
  activityTimer.Start(250);
  Bind(EVT_MVR_GDTF_ANALYSIS, [this](wxThreadEvent &event) {
    if (stop)
      return;
    auto progress = event.GetPayload<AnalysisProgress>();
    if (progress.ready) {
      rows[progress.row].resolution = std::move(progress.resolution);
      rows[progress.row].evidence = std::move(progress.evidence);
      if (rows[progress.row].resolution.selected.source == Source::Catalog) {
        rows[progress.row].catalogProposal = rows[progress.row].resolution.selected;
        rows[progress.row].catalogProposalPending = true;
      }
      profileLabels.insert(progress.profileLabels.begin(), progress.profileLabels.end());
      RefreshRows();
    }
    SetWorkPhase(wxString::Format(_("Analyzing GDTF candidates: %s (%zu/%zu)"),
        wxString::FromUTF8(rows[progress.row].request.type), progress.row + 1, rows.size()));
    determinateProgress = true;
    const auto completedRows = progress.row + (progress.ready ? 1 : 0);
    gauge->SetValue(static_cast<int>(100 * completedRows / rows.size()));
  });
  Bind(EVT_MVR_GDTF_DONE, &MvrGdtfResolutionDialog::OnWorkerDone, this);
  Bind(EVT_MVR_GDTF_PROGRESS, [this](wxThreadEvent &event) {
    if (stop)
      return;
    const auto bytes = event.GetPayload<std::pair<long long, long long>>();
    for (auto &row : rows)
      if (row.resolution.status == Status::Downloading)
        Progress(row.request.type) = {bytes.first, bytes.second};
    RefreshBytesSummary();
    SetWorkPhase(wxString::Format(_("%lld / %lld bytes"), bytes.first, bytes.second));
    determinateProgress = bytes.second > 0;
    if (bytes.second > 0)
      gauge->SetValue(static_cast<int>(std::clamp(
          100.0 * bytes.first / bytes.second, 0.0, 100.0)));
    else
      gauge->Pulse();
  });
  CentreOnParent();
}

MvrGdtfResolutionDialog::~MvrGdtfResolutionDialog() {
  activityTimer.Stop();
  stop = true;
  if (worker.joinable())
    worker.join();
}

void MvrGdtfResolutionDialog::BuildLayout() {
  auto *layout = GetSizer();
  std::size_t insertAt = layout->GetItemCount() - 1;
  phase = progressPhaseText;
  gauge = progressGauge;
  table = downloadInfoList;
  for (const auto &row : rows)
    AddRow(row.request.type);
  table->EnableSourceChoices([this](unsigned row, unsigned source) { ChooseSource(row, source); });
  auto *choices = new wxBoxSizer(wxHORIZONTAL);
  candidates = new wxChoice(this, wxID_ANY);
  resources = new wxChoice(this, wxID_ANY);
  choices->Add(new wxStaticText(this, wxID_ANY, _("Catalog candidates:")), 0,
               wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  choices->Add(candidates, 2, wxRIGHT, 12);
  choices->Add(resources, 1);
  layout->Insert(insertAt++, choices, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
  details = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                            wxSize(-1, 110), wxTE_MULTILINE | wxTE_READONLY);
  layout->Insert(insertAt++, details, 0, wxEXPAND | wxALL, 8);
  auto *actions = new wxBoxSizer(wxHORIZONTAL);
  search = new wxButton(this, wxID_ANY, _("Search GDTF Share..."));
  local = new wxButton(this, wxID_ANY, _("Load local GDTF..."));
  actions->Add(search, 0, wxRIGHT, 8);
  actions->Add(local, 0, wxRIGHT, 8);
  chooseMode = new wxButton(this, wxID_ANY, _("Choose DMX mode..."));
  actions->Add(chooseMode, 0);
  layout->Insert(insertAt++, actions, 0, wxALL, 8);
  ack = ackButton;
  cancel = cancelButton;
  SetMinSize(wxSize(900, 640));
  SetSize(wxSize(1140, 740));
  table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &) { RefreshSelection(); });
  chooseMode->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ChooseReplacementMode(); });
  candidates->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { ChooseCandidate(); });
  resources->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { ChooseResource(); });
  search->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Search(); });
  local->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { LoadLocal(); });
  cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { RequestCancel(); });
  ack->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Acknowledge(); });
  ack->SetDefault();
}

std::optional<std::size_t> MvrGdtfResolutionDialog::SelectedRow() const {
  const long row = table->SelectedRow();
  if (row < 0 || static_cast<std::size_t>(row) >= rows.size())
    return {};
  return static_cast<std::size_t>(row);
}

void MvrGdtfResolutionDialog::SetWorkPhase(const wxString &label) {
  if (workPhase != label) {
    workPhase = label;
    phaseClock.Start();
  }
  phase->SetLabel(label);
}

void MvrGdtfResolutionDialog::SetBusy(bool value) {
  busy = value;
  const bool pending = std::any_of(rows.begin(), rows.end(), [](const Row &row) {
    return row.resolution.status == Status::Pending || row.resolution.status == Status::ModeRequired;
  });
  finished = !busy && !pending;
  ack->Enable(!busy && !stop);
  cancel->Enable(!stop);
  RefreshSelection();
}

void MvrGdtfResolutionDialog::RefreshRows() {
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto &result = rows[i].resolution;
    const auto &choice = result.selected;
    std::string label = choice.revisionId;
    if (choice.source == Source::Catalog) {
      for (const auto &candidate : result.ranking.candidates)
        if (candidate.rid == choice.revisionId) {
          label = candidate.manufacturer + " / " + candidate.fixtureName +
              " / " + candidate.rid;
          break;
        }
    } else if (const auto found = profileLabels.find(choice.path); found != profileLabels.end()) {
      label = found->second;
    }
    const wxString identity = wxString::FromUTF8(label + " / " + choice.mode);
    table->SetItem(i, 1, identity);
    DownloadRowState state = DownloadRowState::Unresolved;
    switch (result.status) {
    case Status::Pending: state = DownloadRowState::Pending; break;
    case Status::Downloading: state = DownloadRowState::Downloading; break;
    case Status::Downloaded: state = DownloadRowState::Downloaded; break;
    case Status::ModeRequired: state = DownloadRowState::ModeRequired; break;
    case Status::Identical: state = DownloadRowState::Identical; break;
    case Status::Reused: state = DownloadRowState::Reused; break;
    case Status::OriginalMvr: state = DownloadRowState::OriginalMvr; break;
    case Status::Application: state = DownloadRowState::Application; break;
    case Status::Local: state = DownloadRowState::Local; break;
    case Status::DownloadFailed: state = DownloadRowState::DownloadFailed; break;
    case Status::Canceled: state = DownloadRowState::Canceled; break;
    default: break;
    }
    const auto status = rows[i].automaticModeUnresolved && result.status == Status::OriginalMvr
        ? _("Downloaded; original MVR retained (DMX mode unresolved)") : StatusText(result.status);
    UpdateStatusRow(rows[i].request.type, identity, status,
                    FormatBytes(Progress(rows[i].request.type).downloadedBytes),
                    wxString::FromUTF8(result.applied.mode), state);
    const auto has = [&](Source source) {
      return std::any_of(result.availableResources.begin(), result.availableResources.end(),
                          [&](const auto &resource) { return resource.source == source; });
    };
    const int selectedSource = result.selected.source == Source::OriginalMvr ? 0 :
        result.selected.source == Source::Application ? 1 :
        (result.selected.source == Source::Catalog || result.selected.source == Source::Local) ? 2 : -1;
    table->SetSourceChoices(i, {has(Source::OriginalMvr), has(Source::Application),
                               rows[i].replacement.has_value() || rows[i].catalogProposal.has_value()},
                            selectedSource, !busy && !stop);
  }
  if (!SelectedRow() && !rows.empty())
    table->SelectRow(0);
  RefreshSelection();
}

void MvrGdtfResolutionDialog::RefreshSelection() {
  const auto selected = SelectedRow();
  candidates->Clear();
  resources->Clear();
  resourceChoices.clear();
  for (auto *control : std::vector<wxWindow *>{candidates, resources, search, local})
    control->Enable(!busy && selected.has_value());
  chooseMode->Enable(!busy && selected && rows[*selected].replacement.has_value());
  if (!selected)
    return;
  const auto &row = rows[*selected];
  for (const auto &candidate : row.resolution.ranking.candidates) {
    wxString modes;
    for (const auto &mode : candidate.modes) {
      if (!modes.empty())
        modes += "; ";
      modes += mode.footprint > 0
          ? wxString::Format(wxPLURAL("%s / %d channel", "%s / %d channels", mode.footprint), wxString::FromUTF8(mode.name), mode.footprint)
          : wxString::Format(_("%s / channel count unavailable"), wxString::FromUTF8(mode.name));
    }
    if (modes.empty())
      modes = _("Mode and channel metadata unavailable");
    candidates->Append(wxString::Format(_("%s / %s / revision %s / %s / %s"),
        wxString::FromUTF8(candidate.manufacturer), wxString::FromUTF8(candidate.fixtureName),
        wxString::FromUTF8(candidate.rid), wxString::FromUTF8(candidate.revision), modes));
    if (row.resolution.selected.source == Source::Catalog &&
        row.resolution.selected.revisionId == candidate.rid)
      candidates->SetSelection(candidates->GetCount() - 1);
  }
  for (const auto &resource : row.resolution.availableResources) {
    resources->Append(wxString::Format(_("%s / %s"), SourceText(resource.source), resource.mode.empty() ? _("Preserve imported modes") : wxString::FromUTF8(resource.mode)));
    resourceChoices.push_back(resource);
    if (resource == row.resolution.selected ||
        (resource.source == Source::Catalog && row.resolution.selected.source == Source::Catalog &&
         resource.revisionId == row.resolution.selected.revisionId && resource.mode == row.resolution.selected.mode))
      resources->SetSelection(resources->GetCount() - 1);
  }
  wxString text = wxString::Format(_("Imported mode: %s"), wxString::FromUTF8(row.request.modeName));
  if (row.evidence.requestedFootprint > 0)
    text += " / " + wxString::Format(wxPLURAL("%d channel", "%d channels", row.evidence.requestedFootprint), row.evidence.requestedFootprint);
  if (row.request.patchSpacingHint > 0)
    text += "\n" + wxString::Format(
        wxPLURAL("Repeated patch spacing: %d channel. This is a hint; unused addresses may be included.",
                 "Repeated patch spacing: %d channels. This is a hint; unused addresses may be included.",
                 row.request.patchSpacingHint),
        row.request.patchSpacingHint);
  text += "\n" + SourceText(row.resolution.selected.source);
  if (row.automaticModeUnresolved)
    text += "\n" + _("The file was downloaded, but its DMX mode could not be assigned unambiguously. The original MVR profile and modes are retained.");
  switch (row.automaticModeEvidence) {
  case ModeEvidence::ImportedName:
    text += "\n" + _("DMX mode assigned automatically from the MVR mode name."); break;
  case ModeEvidence::OriginalFootprint:
    text += "\n" + _("DMX mode assigned automatically from the original channel count."); break;
  case ModeEvidence::PatchSpacing:
    text += "\n" + _("DMX mode inferred automatically from repeated patch spacing."); break;
  default: break;
  }
  if (row.resolution.status == Status::ModeRequired)
    text += "\n" + _("The profile was downloaded. Choose its DMX mode or select another source.");
  if (!row.resolution.ranking.downloadRid && row.resolution.origin != SelectionOrigin::User)
    text += "\n" + _("No unambiguous catalog model match. Select a candidate or another source.");
  if (row.identicalToOriginal)
    text += "\n" + _("The downloaded file is byte-for-byte identical to the original MVR profile.");
  if (row.resolution.selected.source == Source::Catalog)
    text += " / " + wxString::FromUTF8(row.resolution.selected.revisionId);
  for (const auto &candidate : row.resolution.ranking.candidates) {
    if (candidate.rid != row.resolution.selected.revisionId)
      continue;
    const bool reviewed = row.automaticModeEvidence != ModeEvidence::None ||
        (row.resolution.origin == SelectionOrigin::User && row.resolution.status != Status::ModeRequired);
    text += " / " + (row.resolution.status == Status::Identical
        ? _("Downloaded; identical to original MVR")
        : row.automaticModeEvidence != ModeEvidence::None ? _("Automatic DMX mode assignment")
        : reviewed ? _("User selection")
        : candidate.automaticSafe ? _("Safe automatic match") : _("Review required"));
    for (const auto &warning : candidate.warnings) {
      if (warning == "manufacturer-mismatch")
        text += "\n" + _("The manufacturer differs from the imported profile.");
      else if (warning == "model-variant-requires-review" || warning == "numeric-model-mismatch")
        text += "\n" + _("The fixture model or variant differs from the imported profile.");
      else if (warning == "mode-requires-review") {
        if (!reviewed && row.resolution.status != Status::Identical)
          text += "\n" + _("DMX mode compatibility requires review.");
        else if (row.resolution.selected.mode != row.request.modeName)
          text += "\n" + _("The selected DMX mode differs from the imported mode.");
      }
      else if (warning == "footprint-mismatch")
        text += "\n" + _("The DMX footprint differs from the imported profile.");
      else if (warning == "footprint-does-not-prove-channel-semantics")
        text += "\n" + _("A matching footprint does not prove DMX channel compatibility.");
      else if (warning == "multiple-imported-modes")
        text += "\n" + _("This fixture type uses several imported DMX modes.");
      else if (warning == "ambiguous-model-candidates")
        text += "\n" + _("Several fixture models are plausible. Select a profile explicitly.");
    }
  }
  if (row.resolution.selected != row.resolution.applied &&
      row.resolution.status != Status::Downloaded && row.resolution.status != Status::Reused)
    text += "\n" + _("Currently retained profile:") + " " +
        SourceText(row.resolution.applied.source) + " / " +
        wxString::FromUTF8(row.resolution.applied.path);
  for (const auto &diagnostic : row.resolution.diagnostics)
    text += "\n" + wxString::FromUTF8(diagnostic);
  details->ChangeValue(text);
}

std::unordered_map<std::string, Resolution> MvrGdtfResolutionDialog::Results() const {
  std::unordered_map<std::string, Resolution> result;
  for (const auto &row : rows) {
    auto resolution = row.resolution;
    if (stop) {
      resolution.applied = row.request.mvrPath.empty() ? Choice{} :
          Choice{Source::OriginalMvr, {}, row.request.mvrPath, {}};
      resolution.selected = resolution.fallback = resolution.applied;
      resolution.status = Status::Canceled;
    }
    result.emplace(row.request.type, std::move(resolution));
  }
  return result;
}

int MvrGdtfResolutionDialog::RunResolution() {
  SetBusy(true);
  return GdtfDownloadQueueDialog::Run([this] { StartCatalog(); }, false);
}

void MvrGdtfResolutionDialog::Acknowledge() {
  if (busy || stop || !IsModal())
    return;
  const bool hasWork = std::any_of(rows.begin(), rows.end(), [](const auto &row) {
    return row.resolution.status == Status::Pending ||
        row.resolution.status == Status::DownloadFailed ||
        row.resolution.status == Status::ModeRequired;
  });
  if (hasWork)
    ApplyAndAccept();
  else
    EndModal(wxID_OK);
}
