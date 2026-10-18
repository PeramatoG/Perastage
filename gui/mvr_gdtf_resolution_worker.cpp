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
#include "filesystem_path_utils.h"
#include <wx/wx.h>
#include <algorithm>
#include <exception>

using namespace gdtf_resolution;

void MvrGdtfResolutionDialog::StartCatalog() {
  SetBusy(true);
  worker = std::thread([this] {
    WorkerResult result;
    result.catalogLoaded = true;
    try {
      result.catalog = callbacks.loadCatalog();
    } catch (const std::exception &error) {
      result.catalog.diagnostic = error.what();
    }
    for (const auto &row : rows) {
      std::vector<Choice> resources;
      Choice fallback;
      for (const auto &[source, path] :
           std::vector<std::pair<Source, std::string>>{
               {Source::OriginalMvr, row.request.mvrPath},
               {Source::Application, row.request.appPath}}) {
        if (path.empty())
          continue;
        const auto profile = ReadProfile(PathUtils::PathFromUtf8(path));
        if (!profile.usable)
          continue;
        result.profileLabels[path] = profile.manufacturer + " / " + profile.model;
        for (const auto &mode : profile.modes) {
          Choice choice{source, {}, path, mode.name};
          resources.push_back(choice);
          const bool compatibleFallback = source == Source::OriginalMvr ||
              row.request.footprint <= 0 || mode.footprint == row.request.footprint;
          if (row.evidence.importedModes.size() <= 1 && mode.name == row.request.modeName &&
              compatibleFallback)
            fallback = choice;
        }
        if (row.evidence.importedModes.size() > 1 &&
            std::all_of(row.evidence.importedModes.begin(), row.evidence.importedModes.end(),
                        [&](const auto &mode) { return HasMode(profile, mode); })) {
          Choice preserve{source, {}, path, {}};
          resources.push_back(preserve);
          if (source == Source::OriginalMvr)
            fallback = preserve;
        }
      }
      auto evidence = row.evidence;
      evidence.catalogSnapshotSource = result.catalog.online ? "online" : "cache";
      evidence.catalogUpdatedAt = result.catalog.updatedAt;
      evidence.catalogPayloadFingerprint = result.catalog.fingerprint;
      evidence.catalogPayloadBytes = result.catalog.payload.size();
      evidence.catalogParsedEntryCount = result.catalog.entries.size();
      result.resolutions.push_back(Analyze(evidence, result.catalog.entries,
                                          std::move(resources), fallback));
    }
    auto *event = new wxThreadEvent(EVT_MVR_GDTF_DONE);
    event->SetPayload(std::move(result));
    wxQueueEvent(this, event);
  });
}

void MvrGdtfResolutionDialog::OnWorkerDone(wxThreadEvent &event) {
  if (worker.joinable())
    worker.join();
  auto result = event.GetPayload<WorkerResult>();
  if (result.catalogLoaded) {
    catalog = std::move(result.catalog);
    profileLabels = std::move(result.profileLabels);
    for (std::size_t i = 0; i < rows.size(); ++i)
      rows[i].resolution = std::move(result.resolutions[i]);
    if (!catalog.diagnostic.empty())
      phase->SetLabel(wxString::FromUTF8(catalog.diagnostic));
  } else {
    auto &resolution = rows[result.row].resolution;
    if (!stop && !result.download.path.empty() && result.download.profile.usable &&
        resolution.origin == SelectionOrigin::User &&
        !HasMode(result.download.profile, result.attempt.mode)) {
      auto actualChoice = result.attempt;
      if (ChooseMode(result.download.profile, actualChoice.mode, true) &&
          ConfirmChoice(actualChoice, &result.download.profile, result.row)) {
        Select(resolution, actualChoice);
        result.attempt = actualChoice;
      } else {
        result.download.path.clear();
        result.download.diagnostic = "DMX mode selection canceled; the existing fallback is retained.";
      }
    }
    if (!result.download.path.empty() &&
        HasMode(result.download.profile, result.attempt.mode)) {
      if (Complete(resolution, result.attempt, result.download.path, result.attempt.mode)) {
        profileLabels[result.download.path] = result.download.profile.manufacturer +
            " / " + result.download.profile.model;
        completed[ReplacementKey(result.attempt)] = std::move(result.download);
      }
    } else if (stop) {
      gdtf_resolution::Cancel(resolution);
    } else {
      Fail(resolution, result.attempt, result.download.diagnostic);
    }
  }
  busy = false;
  RefreshRows();
  ProcessNext();
}

void MvrGdtfResolutionDialog::ProcessNext() {
  if (stop) {
    for (auto &row : rows)
      gdtf_resolution::Cancel(row.resolution);
      SetBusy(false);
    RefreshRows();
    phase->SetLabel(_("Queue canceled. Keeping downloaded fixtures."));
    EndModal(wxID_CANCEL);
    return;
  }
  for (std::size_t i = 0; i < rows.size(); ++i) {
    auto &resolution = rows[i].resolution;
    if (resolution.status != Status::Pending)
      continue;
    const auto attempt = resolution.selected;
    const auto cached = completed.find(ReplacementKey(attempt));
    if (cached != completed.end()) {
      if (HasMode(cached->second.profile, attempt.mode) &&
          std::filesystem::is_regular_file(PathUtils::PathFromUtf8(cached->second.path))) {
        Complete(resolution, attempt, cached->second.path, attempt.mode, true);
        continue;
      }
      completed.erase(cached);
    }
    if (!callbacks.authenticate()) {
      for (auto &row : rows)
        if (row.resolution.status == Status::Pending)
          Fail(row.resolution, row.resolution.selected,
               "GDTF Share authentication unavailable or canceled.");
      break;
    }
    resolution.status = Status::Downloading;
    SetBusy(true);
    RefreshRows();
    worker = std::thread([this, i, attempt] {
      WorkerResult result;
      result.row = i;
      result.attempt = attempt;
      try {
        result.download = callbacks.download(attempt, [this](long long bytes, long long total) {
          auto *event = new wxThreadEvent(EVT_MVR_GDTF_PROGRESS);
          event->SetPayload(std::make_pair(bytes, total));
          wxQueueEvent(this, event);
        }, [this] { return stop.load(); });
        if (!result.download.path.empty() &&
            rows[i].resolution.origin == SelectionOrigin::Automatic) {
          if (!AutomaticProfileCompatible(result.download.profile, rows[i].evidence,
                                          attempt.mode)) {
            result.download.path.clear();
            result.download.diagnostic = "Downloaded package differs from automatic model/mode evidence.";
          }
        }
      } catch (const std::exception &error) {
        result.download.diagnostic = error.what();
      }
      auto *event = new wxThreadEvent(EVT_MVR_GDTF_DONE);
      event->SetPayload(std::move(result));
      wxQueueEvent(this, event);
    });
    return;
  }
  SetBusy(false);
  RefreshRows();
  phase->SetLabel(_("Queue finished."));
  gauge->SetValue(100);
  const bool needsReview = std::any_of(rows.begin(), rows.end(), [](const Row &row) {
    return row.resolution.status == Status::DownloadFailed ||
        row.resolution.status == Status::Unresolved ||
        (row.resolution.origin == SelectionOrigin::Automatic &&
         !row.resolution.ranking.candidates.empty() &&
         !row.resolution.ranking.recommendedRid);
  });
  if (autoClose->GetValue() && !needsReview)
    EndModal(wxID_OK);
}

void MvrGdtfResolutionDialog::Apply() {
  if (busy)
    return;
  stop = false;
  for (auto &row : rows)
    if (row.resolution.status == Status::DownloadFailed)
      Select(row.resolution, row.resolution.selected, row.resolution.origin);
  ProcessNext();
}

void MvrGdtfResolutionDialog::RequestCancel() {
  stop = true;
  cancelRequested = true;
  if (busy) {
    phase->SetLabel(_("Canceling..."));
  } else {
    for (auto &row : rows)
      gdtf_resolution::Cancel(row.resolution);
    EndModal(wxID_CANCEL);
  }
}
