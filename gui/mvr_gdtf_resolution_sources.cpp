/* This file is part of Perastage, licensed under GPL-3.0-or-later. */
#include "mvr_gdtf_resolution_dialog.h"
#include "gdtf_download_queue_table.h"
#include "filesystem_path_utils.h"
#include <wx/wx.h>
#include <algorithm>

using namespace gdtf_resolution;

void MvrGdtfResolutionDialog::FinishDownload(WorkerResult &result) {
  auto &row = rows[result.row];
  auto &resolution = row.resolution;
  if (stop) {
    gdtf_resolution::Cancel(resolution);
    return;
  }
  if (result.download.credentialsNotSaved)
    wxMessageBox(_("The password could not be saved securely. It will be requested again after restart."),
                 _("GDTF Share credentials"), wxOK | wxICON_WARNING, this);
  if (result.download.path.empty() || !result.download.profile.usable) {
    Fail(resolution, result.attempt, result.download.diagnostic);
    return;
  }
  packages[result.attempt.revisionId] = result.download;
  packages[result.attempt.revisionId].credentialsNotSaved = false;
  row.identicalToOriginal = result.identicalToOriginal;
  row.catalogProposalPending = false;
  profileLabels[result.download.path] = result.download.profile.manufacturer +
      " / " + result.download.profile.model;
  for (auto &candidate : resolution.ranking.candidates)
    if (candidate.rid == result.attempt.revisionId)
      candidate.modes = result.download.profile.modes;
  resolution.availableResources.erase(std::remove_if(resolution.availableResources.begin(),
      resolution.availableResources.end(), [](const auto &resource) {
        return resource.source == Source::Catalog;
      }), resolution.availableResources.end());
  for (const auto &mode : result.download.profile.modes)
    resolution.availableResources.push_back({Source::Catalog, result.attempt.revisionId,
                                             result.download.path, mode.name});
  auto choice = result.attempt;
  row.automaticModeUnresolved = false;
  row.automaticModeEvidence = ModeEvidence::None;
  if (resolution.origin == SelectionOrigin::DownloadRequested) {
    const auto mode = ResolveAutomaticMode(result.download.profile, row.evidence,
                                           row.request.patchSpacingHint);
    choice.mode = mode.mode;
    row.automaticModeEvidence = mode.evidence;
  }
  row.replacement = Choice{Source::Catalog, choice.revisionId, result.download.path, choice.mode};
  if (resolution.origin == SelectionOrigin::DownloadRequested && choice.mode.empty()) {
    // Keep the downloaded artifact available, but never block import on a picker.
    row.automaticModeUnresolved = true;
    const auto original = std::find_if(resolution.availableResources.begin(),
        resolution.availableResources.end(), [](const auto &resource) {
          return resource.source == Source::OriginalMvr && resource.mode.empty();
        });
    if (original != resolution.availableResources.end()) {
      const auto preserve = *original;
      Select(resolution, preserve, SelectionOrigin::DownloadRequested);
    } else {
      Fail(resolution, result.attempt, "No unambiguous DMX mode and no usable original MVR profile.");
      resolution.status = Status::Unresolved;
      row.automaticModeUnresolved = false;
    }
    return;
  }
  if (!HasMode(result.download.profile, choice.mode)) {
    table->SelectRow(result.row);
    if (!ChooseMode(result.download.profile, choice.mode, true, result.row) ||
        !ConfirmChoice(choice, &result.download.profile, result.row)) {
      row.replacement->mode.clear();
      resolution.status = Status::ModeRequired;
      resolution.diagnostics.clear();
      return;
    }
    Select(resolution, choice, SelectionOrigin::User);
  }
  if (resolution.selected != choice)
    Select(resolution, choice, resolution.origin);
  if (Complete(resolution, choice, result.download.path, choice.mode, result.reused)) {
    row.replacement->mode = choice.mode;
    row.catalogProposal = choice;
    if (result.identicalToOriginal)
      resolution.status = Status::Identical;
  }
}

void MvrGdtfResolutionDialog::ChooseSource(unsigned index, unsigned source) {
  if (busy || stop || index >= rows.size())
    return;
  auto &row = rows[index];
  auto &resolution = row.resolution;
  row.automaticModeUnresolved = false;
  row.automaticModeEvidence = ModeEvidence::None;
  table->SelectRow(index);
  if (source == 2) {
    if (row.catalogProposalPending && row.catalogProposal) {
      Select(resolution, *row.catalogProposal, SelectionOrigin::User);
    } else if (row.replacement) {
      const auto choice = *row.replacement;
      const auto profile = ReadProfile(PathUtils::PathFromUtf8(choice.path));
      if (!profile.usable || (!choice.mode.empty() && !HasMode(profile, choice.mode))) {
        wxMessageBox(_("The selected GDTF package is not usable."), _("GDTF selection"),
                     wxOK | wxICON_ERROR, this);
        RefreshRows();
        return;
      }
      if (choice.source == Source::Local) {
        Select(resolution, choice);
      } else if (!choice.mode.empty()) {
        auto attempt = choice;
        attempt.path.clear();
        if (Select(resolution, attempt) &&
            Complete(resolution, attempt, choice.path, choice.mode, true) && row.identicalToOriginal)
          resolution.status = Status::Identical;
      } else {
        ChooseReplacementMode();
      }
    } else if (row.catalogProposal) {
      Select(resolution, *row.catalogProposal, SelectionOrigin::User);
    }
  } else {
    const Source requested = source == 0 ? Source::OriginalMvr : Source::Application;
    auto choices = resolution.availableResources;
    auto preferred = std::find_if(choices.begin(), choices.end(), [&](const auto &choice) {
      return choice.source == requested &&
          (requested == Source::OriginalMvr ? choice.mode.empty() : choice.mode == row.request.modeName);
    });
    if (preferred != choices.end()) {
      Select(resolution, *preferred);
    } else {
      auto available = std::find_if(choices.begin(), choices.end(),
          [&](const auto &choice) { return choice.source == requested; });
      if (available != choices.end()) {
        auto choice = *available;
        const auto profile = ReadProfile(PathUtils::PathFromUtf8(choice.path));
        const auto automatic = ResolveAutomaticMode(profile, row.evidence, row.request.patchSpacingHint);
        if (!automatic.mode.empty()) {
          choice.mode = automatic.mode;
          Select(resolution, choice);
        } else if (ChooseMode(profile, choice.mode, true, index) && ConfirmChoice(choice, &profile, index))
          Select(resolution, choice);
      }
    }
  }
  SetBusy(false);
  RefreshRows();
}

void MvrGdtfResolutionDialog::ChooseReplacementMode() {
  const auto selected = SelectedRow();
  if (busy || !selected || !rows[*selected].replacement)
    return;
  auto &row = rows[*selected];
  auto choice = *row.replacement;
  const auto profile = ReadProfile(PathUtils::PathFromUtf8(choice.path));
  if (!ChooseMode(profile, choice.mode, true, *selected) || !ConfirmChoice(choice, &profile, *selected)) {
    RefreshRows();
    return;
  }
  if (choice.source == Source::Catalog) {
    auto attempt = choice;
    attempt.path.clear();
    if (Select(row.resolution, attempt)) {
      Complete(row.resolution, attempt, choice.path, choice.mode, true);
      if (row.identicalToOriginal)
        row.resolution.status = Status::Identical;
    }
  } else {
    row.resolution.availableResources.push_back(choice);
    Select(row.resolution, choice);
  }
  row.replacement = choice;
  row.automaticModeUnresolved = false;
  row.automaticModeEvidence = ModeEvidence::None;
  SetBusy(false);
  RefreshRows();
}
