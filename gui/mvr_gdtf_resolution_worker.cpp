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
#include <chrono>

using namespace gdtf_resolution;

void MvrGdtfResolutionDialog::StartCatalog() {
  SetBusy(true);
  SetWorkPhase(_("Loading GDTF catalog and signing in..."));
  worker = std::thread([this] {
    WorkerResult result;
    result.catalogLoaded = true;
    const auto canceled = [this] { return stop.load(); };
    const auto milliseconds = [](auto started) {
      return std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started).count();
    };
    try {
      const auto catalogStarted = std::chrono::steady_clock::now();
      result.catalog = callbacks.loadCatalog(canceled);
      const auto catalogMs = milliseconds(catalogStarted);
      std::unordered_map<std::string, Profile> profiles;
      for (std::size_t index = 0; index < rows.size() && !stop; ++index) {
        const auto request = rows[index].request;
        const auto originalEvidence = rows[index].evidence;
        auto *started = new wxThreadEvent(EVT_MVR_GDTF_ANALYSIS);
        started->SetPayload(AnalysisProgress{index, {}, {}, {}, false});
        wxQueueEvent(this, started);
        const auto profilesStarted = std::chrono::steady_clock::now();
        std::vector<Choice> resources;
        Choice fallback;
        auto evidence = originalEvidence;
        for (const auto &[source, path] :
             std::vector<std::pair<Source, std::string>>{
                 {Source::OriginalMvr, request.mvrPath},
                 {Source::Application, request.appPath}}) {
          if (stop || path.empty())
            continue;
          auto [cached, inserted] = profiles.try_emplace(path);
          if (inserted)
            cached->second = ReadProfile(PathUtils::PathFromUtf8(path));
          const auto &profile = cached->second;
          if (!profile.usable)
            continue;
          result.profileLabels[path] = profile.manufacturer + " / " + profile.model;
          if (source == Source::OriginalMvr)
            resources.push_back({source, {}, path, {}});
          for (const auto &mode : profile.modes) {
            Choice choice{source, {}, path, mode.name};
            resources.push_back(choice);
            if (source == Source::OriginalMvr && mode.name == request.modeName &&
                mode.footprint > 0 && evidence.importedModes.size() <= 1)
              evidence.requestedFootprint = mode.footprint;
            const bool compatibleFallback = source == Source::OriginalMvr ||
                evidence.requestedFootprint <= 0 || mode.footprint == evidence.requestedFootprint;
            if (evidence.importedModes.size() <= 1 && mode.name == request.modeName &&
                compatibleFallback)
              fallback = choice;
          }
          if (evidence.importedModes.size() > 1 &&
              std::all_of(evidence.importedModes.begin(), evidence.importedModes.end(),
                          [&](const auto &mode) { return HasMode(profile, mode); })) {
            Choice preserve{source, {}, path, {}};
            resources.push_back(preserve);
            if (source == Source::OriginalMvr)
              fallback = preserve;
          }
        }
        if (stop)
          break;
        evidence.catalogSnapshotSource = result.catalog.online ? "online" : "cache";
        evidence.catalogUpdatedAt = result.catalog.updatedAt;
        evidence.catalogPayloadFingerprint = result.catalog.fingerprint;
        evidence.catalogPayloadBytes = result.catalog.payload.size();
        evidence.catalogParsedEntryCount = result.catalog.entries.size();
        const auto profilesMs = milliseconds(profilesStarted);
        const auto rankingStarted = std::chrono::steady_clock::now();
        auto resolution = Analyze(evidence, result.catalog.entries,
                                  std::move(resources), fallback, {}, canceled,
                                  CatalogIntent::DownloadRequested);
        resolution.ranking.diagnostics += "; catalog_load_ms=" + std::to_string(catalogMs) +
            "; profile_read_ms=" + std::to_string(profilesMs) +
            "; ranking_ms=" + std::to_string(milliseconds(rankingStarted));
        if (stop)
          break;
        auto *progress = new wxThreadEvent(EVT_MVR_GDTF_ANALYSIS);
        progress->SetPayload(AnalysisProgress{index, std::move(resolution),
                                             result.profileLabels, std::move(evidence)});
        wxQueueEvent(this, progress);
      }
    } catch (const std::exception &error) {
      result.catalog.diagnostic = error.what();
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
    if (!catalog.diagnostic.empty())
      phase->SetLabel(wxString::FromUTF8(catalog.diagnostic));
  } else {
    FinishDownload(result);
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
    SetWorkPhase(_("Queue canceled. Original MVR profiles retained."));
    EndModal(wxID_CANCEL);
    return;
  }
  for (std::size_t i = 0; i < rows.size(); ++i) {
    auto &resolution = rows[i].resolution;
    if (resolution.status != Status::Pending)
      continue;
    const auto attempt = resolution.selected;
    std::optional<MvrGdtfDownload> cached;
    if (const auto found = packages.find(attempt.revisionId); found != packages.end()) {
      if (std::filesystem::is_regular_file(PathUtils::PathFromUtf8(found->second.path)))
        cached = found->second;
      else
        packages.erase(found);
    }
    if (!cached && !callbacks.authenticate()) {
      for (auto &row : rows)
        if (row.resolution.status == Status::Pending)
          Fail(row.resolution, row.resolution.selected,
               "GDTF Share authentication unavailable or canceled.");
      break;
    }
    resolution.status = Status::Downloading;
    SetBusy(true);
    RefreshRows();
    determinateProgress = false;
    SetWorkPhase(wxString::Format(_("Connecting and downloading: %s"),
                                  wxString::FromUTF8(rows[i].request.type)));
    const auto evidence = rows[i].evidence;
    const auto origin = resolution.origin;
    const auto originalPath = rows[i].request.mvrPath;
    worker = std::thread([this, i, attempt, evidence, origin, originalPath, cached] {
      WorkerResult result;
      result.row = i;
      result.attempt = attempt;
      try {
        result.reused = cached.has_value();
        if (cached)
          result.download = *cached;
        else
          result.download = callbacks.download(attempt, [this](long long bytes, long long total) {
          auto *event = new wxThreadEvent(EVT_MVR_GDTF_PROGRESS);
          event->SetPayload(std::make_pair(bytes, total));
          wxQueueEvent(this, event);
        }, [this] { return stop.load(); });
        if (!result.download.path.empty()) {
          const bool compatible = origin == SelectionOrigin::Automatic
              ? AutomaticProfileCompatible(result.download.profile, evidence, attempt.mode)
              : origin != SelectionOrigin::DownloadRequested ||
                  ProfileIdentityCompatible(result.download.profile, evidence);
          if (!compatible) {
            result.download.path.clear();
            result.download.diagnostic = "Downloaded package differs from catalog model evidence.";
          } else if (!stop) {
            result.identicalToOriginal = SameProfileBytes(PathUtils::PathFromUtf8(originalPath),
                PathUtils::PathFromUtf8(result.download.path));
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
  const bool missingModes = std::any_of(rows.begin(), rows.end(), [](const auto &row) {
    return row.resolution.status == Status::ModeRequired;
  });
  phase->SetLabel(missingModes ? _("Downloads finished. Choose a DMX mode or another source.")
                               : _("Queue finished."));
  gauge->SetValue(100);
  const bool needsReview = std::any_of(rows.begin(), rows.end(), [](const Row &row) {
    return row.resolution.status == Status::ModeRequired ||
        row.resolution.status == Status::DownloadFailed ||
        row.resolution.status == Status::Unresolved ||
        (row.resolution.origin == SelectionOrigin::Automatic &&
         !row.resolution.ranking.downloadRid);
  });
  if (automaticClose && !needsReview)
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
    cancel->Disable();
    SetWorkPhase(_("Canceling..."));
  } else {
    for (auto &row : rows)
      gdtf_resolution::Cancel(row.resolution);
    EndModal(wxID_CANCEL);
  }
}
