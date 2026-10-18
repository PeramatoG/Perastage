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
#include "gdtfsearchdialog.h"
#include "filesystem_path_utils.h"
#include "wx_path_utils.h"
#include <wx/wx.h>
#include <wx/choice.h>
#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <algorithm>

using namespace gdtf_resolution;

bool MvrGdtfResolutionDialog::ChooseMode(const Profile &profile, std::string &mode,
                                        bool alwaysAsk) {
  if (!profile.usable || profile.modes.empty())
    return false;
  if (!alwaysAsk && HasMode(profile, mode))
    return true;
  if (profile.modes.size() == 1) {
    mode = profile.modes.front().name;
    return true;
  }
  wxArrayString choices;
  int initial = 0;
  const auto selected = SelectedRow();
  const int footprint = !selected ? 0 : rows[*selected].evidence.requestedFootprint > 0
      ? rows[*selected].evidence.requestedFootprint : rows[*selected].request.patchSpacingHint;
  if (!HasMode(profile, mode) && footprint > 0)
    for (std::size_t i = 0; i < profile.modes.size(); ++i)
      if (profile.modes[i].footprint == footprint) {
        initial = static_cast<int>(i);
        break;
      }
  for (std::size_t i = 0; i < profile.modes.size(); ++i) {
    const auto &item = profile.modes[i];
    choices.Add(item.footprint > 0
        ? wxString::Format(wxPLURAL("%s / %d channel", "%s / %d channels", item.footprint), wxString::FromUTF8(item.name), item.footprint)
        : wxString::Format(_("%s / channel count unavailable"), wxString::FromUTF8(item.name)));
    if (item.name == mode)
      initial = static_cast<int>(i);
  }
  wxSingleChoiceDialog dialog(this, _("Select the DMX mode for this profile."),
                               _("DMX mode"), choices);
  dialog.SetSelection(initial);
  if (dialog.ShowModal() != wxID_OK)
    return false;
  mode = profile.modes[dialog.GetSelection()].name;
  return true;
}

bool MvrGdtfResolutionDialog::ConfirmChoice(const Choice &choice, const Profile *profile,
                                            std::optional<std::size_t> rowIndex) {
  if (callbacks.confirmSelection)
    return callbacks.confirmSelection(choice, profile);
  const auto selected = rowIndex ? rowIndex : SelectedRow();
  if (!selected)
    return false;
  const auto &row = rows[*selected];
  wxString warning = _("Review model, mode and channel compatibility before applying.");
  if (profile)
    warning += "\n" + wxString::FromUTF8(profile->manufacturer + " / " + profile->model);
  warning += "\n" + wxString::FromUTF8(choice.revisionId + " / " + choice.mode);
  if (row.evidence.importedModes.size() > 1 && !choice.mode.empty())
    warning += "\n" + _("The selected DMX mode will apply to every instance of this fixture type.");
  if (!choice.mode.empty() && choice.mode != row.request.modeName)
    warning += "\n" + _("The selected DMX mode differs from the imported mode.");
  if (profile) {
    const auto mode = std::find_if(profile->modes.begin(), profile->modes.end(),
        [&](const auto &item) { return item.name == choice.mode; });
    if (mode != profile->modes.end() && row.evidence.requestedFootprint > 0 &&
        mode->footprint > 0 && mode->footprint != row.evidence.requestedFootprint)
      warning += "\n" + _("The DMX footprint differs from the imported profile.");
  }
  return wxMessageBox(warning, _("Confirm GDTF selection"),
                       wxOK | wxCANCEL | wxICON_WARNING, this) == wxOK;
}

void MvrGdtfResolutionDialog::ChooseCandidate() {
  const auto row = SelectedRow();
  const int index = candidates->GetSelection();
  if (busy || !row || index == wxNOT_FOUND)
    return;
  const auto &candidate = rows[*row].resolution.ranking.candidates[index];
  mvr::gdtf_catalog_matcher::GdtfCatalogEntry entry;
  entry.rid = candidate.rid;
  entry.manufacturer = candidate.manufacturer;
  entry.fixtureName = candidate.fixtureName;
  entry.modes = candidate.modes;
  entry.downloadable = true;
  SelectCatalog(entry);
}

void MvrGdtfResolutionDialog::SelectCatalog(
    const mvr::gdtf_catalog_matcher::GdtfCatalogEntry &entry) {
  const auto row = SelectedRow();
  if (!row || !entry.downloadable || entry.rid.empty())
    return;
  Profile profile;
  profile.usable = !entry.modes.empty();
  profile.manufacturer = entry.manufacturer;
  profile.model = entry.fixtureName;
  profile.modes = entry.modes;
  std::string mode = rows[*row].request.modeName;
  if (profile.modes.empty()) {
    // Select the actual package mode after download when catalog metadata is absent.
    mode.clear();
  } else if (!ChooseMode(profile, mode, true)) {
    RefreshSelection();
    return;
  }
  Choice choice{Source::Catalog, entry.rid, {}, mode};
  if (!ConfirmChoice(choice, &profile)) {
    RefreshSelection();
    return;
  }
  // Keep manually browsed revisions in this row even when outside its top eight.
  auto &ranking = rows[*row].resolution.ranking;
  for (auto &candidate : ranking.candidates) {
    if (candidate.rid != entry.rid)
      continue;
    if (candidate.selectedMode != mode) {
      candidate.automaticSafe = false;
      candidate.confidence = mvr::gdtf_catalog_matcher::MatchConfidence::Review;
      candidate.warnings.push_back("mode-requires-review");
    }
    candidate.selectedMode = mode;
    for (const auto &available : profile.modes)
      if (available.name == mode)
        candidate.footprint = available.footprint;
  }
  if (std::none_of(ranking.candidates.begin(), ranking.candidates.end(),
                  [&](const auto &candidate) { return candidate.rid == entry.rid; })) {
    mvr::gdtf_catalog_matcher::RankedCandidate candidate;
    candidate.rid = entry.rid;
    candidate.manufacturer = entry.manufacturer;
    candidate.fixtureName = entry.fixtureName;
    candidate.revision = entry.revision;
    candidate.modes = entry.modes;
    candidate.selectedMode = mode;
    candidate.warnings = {"explicit-catalog-selection"};
    ranking.candidates.push_back(std::move(candidate));
  }
  Select(rows[*row].resolution, choice);
  RefreshRows();
  SetBusy(false);
}

void MvrGdtfResolutionDialog::ChooseResource() {
  const auto row = SelectedRow();
  const int index = resources->GetSelection();
  if (busy || !row || index == wxNOT_FOUND)
    return;
  const auto choice = resourceChoices[index];
  const auto profile = ReadProfile(PathUtils::PathFromUtf8(choice.path));
  if (profile.usable && (choice.mode.empty() || HasMode(profile, choice.mode)) &&
      ConfirmChoice(choice, &profile))
    Select(rows[*row].resolution, choice);
  RefreshRows();
  SetBusy(false);
}

void MvrGdtfResolutionDialog::Search() {
  const auto row = SelectedRow();
  if (busy || !row)
    return;
  if (catalog.entries.empty() && !callbacks.authenticate())
    return;
  // The existing browser returns a selection; the import queue owns downloading.
  GdtfSearchDialog dialog(this, catalog.payload, catalog.updatedAt, [this] {
    auto loaded = callbacks.loadCatalog([this] { return stop.load(); });
    GdtfSearchDialog::RefreshResult result;
    result.success = !loaded.entries.empty();
    result.listData = loaded.payload;
    result.updatedAt = loaded.updatedAt;
    mvr::gdtf_catalog_parser::GdtfCatalogParseResult parsed;
    parsed.entries = std::move(loaded.entries);
    result.parsedCatalog = std::move(parsed);
    result.failureDetails = loaded.diagnostic;
    result.source = loaded.online ? GdtfCatalogDisplaySource::Online
                                  : GdtfCatalogDisplaySource::Cached;
    return result;
  }, catalog.online ? GdtfCatalogDisplaySource::Online : GdtfCatalogDisplaySource::Cached,
     false, rows[*row].request.fixtureName, catalog.entries, true);
  if (dialog.ShowModal() == wxID_OK)
    if (const auto entry = dialog.GetSelectedEntry())
      SelectCatalog(*entry);
}

void MvrGdtfResolutionDialog::LoadLocal() {
  const auto row = SelectedRow();
  if (busy || !row)
    return;
  wxFileDialog picker(this, _("Load local GDTF..."), {}, {},
                       _("GDTF files (*.gdtf)|*.gdtf"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (picker.ShowModal() != wxID_OK)
    return;
  const auto source = WxPathUtils::FilesystemPathFromWxString(picker.GetPath());
  const auto profile = ReadProfile(source);
  if (!profile.usable) {
    wxString diagnostic = _("The selected GDTF package is not usable.");
    for (const auto &message : profile.diagnostics)
      diagnostic += "\n" + wxString::FromUTF8(message);
    wxMessageBox(diagnostic, _("GDTF selection"), wxOK | wxICON_ERROR, this);
    return;
  }
  std::string mode = rows[*row].request.modeName;
  if (!ChooseMode(profile, mode, true))
    return;
  Choice choice{Source::Local, {}, {}, mode};
  if (!ConfirmChoice(choice, &profile))
    return;
  std::string diagnostic;
  const auto destination = CopyIntoScene(source, sceneRoot, diagnostic);
  if (destination.empty()) {
    wxMessageBox(wxString::FromUTF8(diagnostic), _("GDTF selection"), wxOK | wxICON_ERROR, this);
    return;
  }
  choice.path = PathUtils::PathToUtf8(destination);
  profileLabels[choice.path] = profile.manufacturer + " / " + profile.model +
      " / " + PathUtils::PathToUtf8(source.filename());
  rows[*row].resolution.availableResources.push_back(choice);
  Select(rows[*row].resolution, choice);
  RefreshRows();
  SetBusy(false);
}
