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
#pragma once
#include "gdtf_resolution.h"
#include "gdtf_resolution_resources.h"
#include "../mvr/mvr_scene_node_reader.h"
#include "gdtf_download_queue_dialog.h"
#include <atomic>
#include <functional>
#include <thread>
#include <unordered_map>
#include <wx/timer.h>
#include <wx/stopwatch.h>

class wxListCtrl;
class wxChoice;
class wxStaticText;
class wxTextCtrl;
class wxButton;
class wxGauge;
class wxThreadEvent;
wxDECLARE_EVENT(EVT_MVR_GDTF_DONE, wxThreadEvent);
wxDECLARE_EVENT(EVT_MVR_GDTF_PROGRESS, wxThreadEvent);
wxDECLARE_EVENT(EVT_MVR_GDTF_ANALYSIS, wxThreadEvent);

struct MvrGdtfCatalog {
  std::string payload;
  std::string updatedAt;
  std::string fingerprint;
  std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> entries;
  bool online = false;
  std::string diagnostic;
};
struct MvrGdtfDownload {
  std::string path;
  gdtf_resolution::Profile profile;
  std::string diagnostic;
  bool credentialsNotSaved = false;
};
struct MvrGdtfResolutionCallbacks {
  std::function<MvrGdtfCatalog(std::function<bool()>)> loadCatalog;
  std::function<bool()> authenticate;
  std::function<bool(const gdtf_resolution::Choice &,
                     const gdtf_resolution::Profile *)> confirmSelection;
  std::function<MvrGdtfDownload(
      const gdtf_resolution::Choice &, std::function<void(long long, long long)>,
      std::function<bool()>)> download;
};

// Native presentation; typed resolution and package validation stay in Core.
class MvrGdtfResolutionDialog final : public GdtfDownloadQueueDialog {
public:
  MvrGdtfResolutionDialog(wxWindow *parent,
      const std::vector<mvr::SceneReadGdtfConflict> &requests,
      const std::filesystem::path &sceneRoot,
      MvrGdtfResolutionCallbacks callbacks, bool automaticClose,
      const std::unordered_map<std::string, std::vector<std::string>> &importedModes = {});
  ~MvrGdtfResolutionDialog() override;
  int RunResolution();
  std::unordered_map<std::string, gdtf_resolution::Resolution> Results() const;

private:
  struct Row {
    mvr::SceneReadGdtfConflict request;
    mvr::gdtf_catalog_matcher::GdtfDownloadRequest evidence;
    gdtf_resolution::Resolution resolution;
  };
  struct WorkerResult {
    bool catalogLoaded = false;
    MvrGdtfCatalog catalog;
    std::unordered_map<std::string, std::string> profileLabels;
    std::size_t row = 0;
    gdtf_resolution::Choice attempt;
    MvrGdtfDownload download;
  };
  struct AnalysisProgress {
    std::size_t row = 0;
    gdtf_resolution::Resolution resolution;
    std::unordered_map<std::string, std::string> profileLabels;
    mvr::gdtf_catalog_matcher::GdtfDownloadRequest evidence;
    bool ready = true;
  };
  void BuildLayout();
  void SetWorkPhase(const wxString &label);
  void StartCatalog();
  void OnWorkerDone(wxThreadEvent &event);
  void ProcessNext();
  void RefreshRows();
  void RefreshSelection();
  void ChooseCandidate();
  void ChooseResource();
  void Search();
  void LoadLocal();
  void Apply();
  void RequestCancel() override;
  bool CancelDiscardsSelections() const override { return true; }
  void Acknowledge() override;
  bool ChooseMode(const gdtf_resolution::Profile &profile, std::string &mode,
                  bool alwaysAsk = false);
  bool ConfirmChoice(const gdtf_resolution::Choice &choice,
                     const gdtf_resolution::Profile *profile = nullptr,
                     std::optional<std::size_t> rowIndex = {});
  void SelectCatalog(const mvr::gdtf_catalog_matcher::GdtfCatalogEntry &entry);
  std::optional<std::size_t> SelectedRow() const;
  void SetBusy(bool value);

  std::vector<Row> rows;
  std::filesystem::path sceneRoot;
  MvrGdtfResolutionCallbacks callbacks;
  MvrGdtfCatalog catalog;
  std::unordered_map<std::string, MvrGdtfDownload> completed;
  std::vector<gdtf_resolution::Choice> resourceChoices;
  std::unordered_map<std::string, std::string> profileLabels;
  std::thread worker;
  std::atomic<bool> stop{false};
  bool busy = false;
  wxListCtrl *table = nullptr;
  wxChoice *candidates = nullptr;
  wxChoice *resources = nullptr;
  wxTextCtrl *details = nullptr;
  wxStaticText *phase = nullptr;
  wxGauge *gauge = nullptr;
  bool automaticClose = false;
  wxTimer activityTimer;
  wxStopWatch phaseClock;
  wxString workPhase;
  bool determinateProgress = false;
  wxButton *search = nullptr;
  wxButton *local = nullptr;
  wxButton *apply = nullptr;
  wxButton *ack = nullptr;
  wxButton *cancel = nullptr;
};
