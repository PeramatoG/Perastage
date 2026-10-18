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
#include "mvr_gdtf_resolution_workflow.h"
#include "mvr_gdtf_resolution_dialog.h"
#include "credentialstore.h"
#include "gdtf_download_workflow.h"
#include "gdtf_catalog_service.h"
#include "filesystem_path_utils.h"
#include "guiconfigservices.h"
#include "logindialog.h"
#include "runtime_storage.h"
#include "../mvr/mvr_preferences.h"
#include <wx/wx.h>
#include <wx/datetime.h>
#include <memory>

std::unordered_map<std::string, gdtf_resolution::Resolution>
RunMvrGdtfResolution(wxWindow *parent,
    const std::vector<mvr::SceneReadGdtfConflict> &requests,
    const std::filesystem::path &sceneRoot,
    std::function<void(const std::string &)> log,
    const std::unordered_map<std::string, std::vector<std::string>> &importedModes) {
  // Secure-store IO and network requests start after the modal queue is visible.
  CredentialStore::LoadResult loaded;
  std::optional<CredentialStore::Credentials> credentials;
  bool saveCredentials = false;
  bool credentialsLoaded = false;
  GdtfShareClient client;
  GdtfShareResult lastLogin;
  auto login = [&](const std::function<bool()> &canceled) {
    if (!credentials || canceled())
      return false;
    client.ResetSession();
    lastLogin = client.Login(credentials->username, credentials->password, canceled);
    return lastLogin.Succeeded();
  };
  MvrGdtfResolutionCallbacks callbacks;
  // Only native credential entry belongs on the GUI thread. Authentication and
  // secure-store writes are performed by the subsequent download worker.
  callbacks.authenticate = [&] {
    if (client.IsAuthenticated() ||
        (credentials && lastLogin.category != GdtfShareResultCategory::AuthenticationRejected))
      return true;
    GdtfLoginDialog dialog(parent, credentials ? credentials->username
                                             : loaded.usernameHint.value_or(""),
                            credentials ? credentials->password : "");
    if (dialog.ShowModal() != wxID_OK)
      return false;
    credentials = CredentialStore::Credentials{dialog.GetUsername(), dialog.GetPassword()};
    saveCredentials = true;
    return true;
  };
  callbacks.loadCatalog = [&](std::function<bool()> canceled) {
    if (canceled())
      return MvrGdtfCatalog{};
    if (!credentialsLoaded) {
      loaded = CredentialStore::LoadDetailed();
      credentials = loaded.credentials;
      credentialsLoaded = true;
    }
    GdtfCatalogService service;
    MvrGdtfCatalog catalog;
    const auto now = wxDateTime::UNow().FormatISOCombined(' ').ToStdString();
    auto result = service.RefreshCatalogIfStale([&](std::string &payload) {
      if (canceled() || (!client.IsAuthenticated() && !login(canceled)))
        return false;
      auto response = client.GetCatalog(canceled);
      if (response.category == GdtfShareResultCategory::AuthenticationRejected && !canceled() && login(canceled))
        response = client.GetCatalog(canceled);
      payload = std::move(response.payload);
      return response.Succeeded() && !payload.empty();
    }, now);
    if (result.snapshot) {
      catalog.payload = std::move(result.snapshot->listData);
      catalog.updatedAt = std::move(result.snapshot->updatedAt);
    }
    if (result.parsedCatalog) {
      catalog.fingerprint = result.parsedCatalog->payloadFingerprint;
      catalog.entries = std::move(result.parsedCatalog->entries);
    }
    catalog.online = result.source == GdtfCatalogResultSource::Online;
    catalog.diagnostic = result.failureMessage;
    return catalog;
  };
  callbacks.download = [&](const gdtf_resolution::Choice &choice,
      std::function<void(long long, long long)> progress, std::function<bool()> canceled) {
    MvrGdtfDownload result;
    if (canceled())
      return result;
    if (!client.IsAuthenticated() && !login(canceled)) {
      result.diagnostic = FormatGdtfShareUserMessage(lastLogin, "login");
      return result;
    }
    if (canceled())
      return result;
    if (saveCredentials) {
      const auto saved = CredentialStore::Save(*credentials);
      saveCredentials = false;
      result.credentialsNotSaved = !saved.Succeeded();
      if (!saved.Succeeded())
        result.diagnostic = "GDTF Share credentials could not be persisted: " +
            CredentialStore::StatusName(saved.status);
    }
    runtime_storage::TemporaryWorkspace workspace("mvr-gdtf-download");
    const auto temporary = workspace.Path() / "profile.gdtf";
    const auto downloaded = gdtf_download_workflow::DownloadWithExpiredSessionRetry(
        client, choice.revisionId, PathUtils::PathToUtf8(temporary), [&] { return login(canceled); },
        [&](const GdtfDownloadProgress &bytes) { progress(bytes.downloadedBytes, bytes.totalBytes); },
        canceled);
    if (!downloaded.Succeeded()) {
      result.diagnostic = FormatGdtfShareUserMessage(downloaded, "download");
      return result;
    }
    if (canceled())
      return result;
    result.profile = gdtf_resolution::ReadProfile(temporary);
    if (!result.profile.usable) {
      result.diagnostic = "Downloaded GDTF is not a usable fixture profile.";
      for (const auto &message : result.profile.diagnostics)
        result.diagnostic += "\n" + message;
      return result;
    }
    if (canceled())
      return result;
    const auto destination = gdtf_resolution::CopyIntoScene(temporary, sceneRoot, result.diagnostic);
    result.path = PathUtils::PathToUtf8(destination);
    return result;
  };
  const bool automaticClose = mvr::preferences::AutoCloseGdtfDownloadWindow(
      GetDefaultGuiConfigServices().Preferences());
  MvrGdtfResolutionDialog dialog(parent, requests, sceneRoot, std::move(callbacks),
                                  automaticClose, importedModes);
  const bool accepted = dialog.RunResolution() == wxID_OK;
  auto results = dialog.Results();
  if (!accepted)
    for (const auto &request : requests) {
      auto &result = results[request.type];
      result.applied = request.mvrPath.empty() ? gdtf_resolution::Choice{} :
          gdtf_resolution::Choice{gdtf_resolution::Source::OriginalMvr, {}, request.mvrPath, {}};
      result.selected = result.fallback = result.applied;
      result.status = gdtf_resolution::Status::Canceled;
    }
  for (const auto &[type, result] : results) {
    log("GDTF resolution type='" + type + "'; selected-rid='" + result.selected.revisionId +
        "'; applied-rid='" + result.applied.revisionId + "'; mode='" + result.applied.mode +
        "'; source=" + std::to_string(static_cast<int>(result.applied.source)));
    log(result.ranking.diagnostics);
    for (const auto &diagnostic : result.diagnostics)
      log(diagnostic);
  }
  return results;
}
