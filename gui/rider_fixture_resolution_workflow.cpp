#include "rider_fixture_resolution_workflow.h"

#include "rider_fixture_resolution_dialog.h"
#include "logindialog.h"
#include "mainwindow_gdtf_credentials.h"
#include "../core/diagnostics/DiagnosticLogger.h"
#include "../core/gdtf_catalog_service.h"
#include "../core/gdtfdictionary.h"
#include "../core/gdtf_download_filename.h"
#include "../core/gdtf_download_workflow.h"
#include "../core/gdtfnet.h"
#include "../core/gdtf_share_workflow.h"
#include "../core/projectutils.h"
#include "../core/riderimporter.h"
#include "../mvr/gdtf_catalog_parser.h"
#include "gdtfloader.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <future>
#include <map>
#include <set>
#include <thread>

#include <wx/datetime.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/utils.h>

namespace rider_fixture_resolution_gui {
namespace {

// Waits for a network task while keeping wxWidgets event processing responsive.
template <typename Result>
Result WaitForNetworkTask(wxWindow *parent, const wxString &title,
                          const wxString &message,
                          std::future<Result> future) {
  wxProgressDialog progress(title, message, 100, parent,
                            wxPD_APP_MODAL | wxPD_SMOOTH);
  int pulse = 0;
  while (future.wait_for(std::chrono::milliseconds(30)) !=
         std::future_status::ready) {
    progress.Update(++pulse % 100, message);
    wxYieldIfNeeded();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
  }
  return future.get();
}

// Prompts for credentials only when authenticated GDTF Share access is needed.
bool EnsureAuthenticated(wxWindow *parent, ConfigManager &configManager,
                         GdtfShareClient &client,
                         std::optional<CredentialStore::Credentials> &credentials) {
  if (client.IsAuthenticated())
    return true;
  const auto loaded = LoadGdtfCredentialsForGuiDetailed(configManager);
  if (!credentials)
    credentials = loaded.credentials;
  while (true) {
    if (!credentials || credentials->username.empty() ||
        credentials->password.empty()) {
      const std::string initialUser = credentials
          ? credentials->username
          : loaded.usernameHint.value_or(std::string());
      GdtfLoginDialog login(parent, initialUser, std::string());
      if (login.ShowModal() != wxID_OK)
        return false;
      credentials = CredentialStore::Credentials{login.GetUsername(),
                                                  login.GetPassword()};
    }
    if (!credentials || credentials->username.empty() ||
        credentials->password.empty())
      return false;
    client.ResetSession();
    const auto result = WaitForNetworkTask(
        parent, _("GDTF Share"), _("Signing in to GDTF Share..."),
        std::async(std::launch::async, [&client, &credentials]() {
          return client.Login(credentials->username, credentials->password);
        }));
    if (result.Succeeded()) {
      const CredentialStore::Result persisted =
          PersistGdtfCredentialsForGui(*credentials, configManager);
      if (!persisted.Succeeded()) {
        wxMessageBox(
            _("GDTF Share authentication succeeded, but the credentials could not be saved securely. You may need to enter them again after restart."),
            _("GDTF Share credentials"), wxOK | wxICON_WARNING, parent);
      }
      return true;
    }
    wxMessageBox(wxString::FromUTF8(FormatGdtfShareUserMessage(result, "login")),
                 _("GDTF Share sign-in unavailable"), wxOK | wxICON_WARNING,
                 parent);
    if (result.category != GdtfShareResultCategory::AuthenticationRejected)
      return false;
    credentials.reset();
  }
}

// Logs stable aggregate counts for the preflight decision.
void LogAnalysisCounts(const rider_fixture_resolution::Analysis &analysis) {
  std::map<rider_fixture_resolution::State, size_t> counts;
  for (const auto &item : analysis.items)
    ++counts[item.state];
  diagnostics::DiagnosticLogger::Info(
      "Rider fixture preflight: unique=" + std::to_string(analysis.items.size()) +
      " dictionary=" + std::to_string(counts[rider_fixture_resolution::State::Dictionary]) +
      " suggested=" + std::to_string(counts[rider_fixture_resolution::State::Suggested]) +
      " review=" + std::to_string(counts[rider_fixture_resolution::State::Review]) +
      " unresolved=" + std::to_string(counts[rider_fixture_resolution::State::Unresolved]) +
      " generic=" + std::to_string(counts[rider_fixture_resolution::State::Generic]));
}

} // namespace

// Runs the complete non-destructive resolution gate before rider scene import.
PreflightResult RunCreateFromTextPreflight(wxWindow *parent,
                                           ConfigManager &configManager,
                                           const std::string &text,
                                           std::string *filteredTextOut,
                                           RiderImporter::ImportPlan *importPlanOut) {
  const auto preflightStarted = std::chrono::steady_clock::now();
  const auto riderStarted = std::chrono::steady_clock::now();
  const RiderImporter::TextAnalysis riderAnalysis =
      RiderImporter::AnalyzeText(text);
  const auto riderAnalysisMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - riderStarted).count();
  if (filteredTextOut)
    *filteredTextOut = riderAnalysis.filteredText;
  const auto &requests = riderAnalysis.fixtureTypes;
  const auto dictionaryStarted = std::chrono::steady_clock::now();
  const auto dictionary = GdtfDictionary::Load().value_or(
      std::unordered_map<std::string, GdtfDictionary::Entry>{});
  const auto dictionaryLoadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - dictionaryStarted).count();
  auto analysis = rider_fixture_resolution::Service::Analyze(requests, dictionary, {});
  LogAnalysisCounts(analysis);
  if (!analysis.RequiresPreflight())
    return PreflightResult::Proceed;

  GdtfCatalogService catalogService;
  auto loadCachedCatalog = []()
      -> std::optional<RiderFixtureResolutionDialog::CatalogData> {
    GdtfCatalogService catalogService;
    const auto cacheStarted = std::chrono::steady_clock::now();
    const auto catalog = catalogService.GetParsedCatalogSnapshot();
    const auto cacheReadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cacheStarted).count();
    if (!catalog)
      return std::nullopt;
    diagnostics::DiagnosticLogger::Info(
        "Rider fixture preflight catalog: catalog_cache_read_ms=" +
        std::to_string(cacheReadMs) + " catalog_parse_ms=" +
        std::to_string(catalog->parsed.parseMs) + " entries=" +
        std::to_string(catalog->parsed.usableEntryCount));
    return RiderFixtureResolutionDialog::CatalogData{
        catalog->snapshot, catalog->parsed.entries,
        RiderFixtureResolutionDialog::CatalogSource::Cached};
  };

  GdtfShareClient client;
  const CredentialStore::LoadResult loadedCredentials =
      LoadGdtfCredentialsForGuiDetailed(configManager);
  std::optional<CredentialStore::Credentials> credentials =
      loadedCredentials.credentials;
  const std::string catalogRefreshTime =
      wxDateTime::UNow().FormatISOCombined(' ').ToStdString();
  auto loadOnlineCatalog =
      [&](const CredentialStore::Credentials &onlineCredentials,
          std::stop_token stopToken,
          RiderFixtureResolutionDialog::OnlineProgressCallback report)
      -> RiderFixtureResolutionDialog::OnlineCatalogResult {
    using OnlineStatus = RiderFixtureResolutionDialog::OnlineCatalogStatus;
    if (stopToken.stop_requested())
      return {OnlineStatus::Unavailable, std::nullopt, "cancelled"};
    report({rider_fixture_resolution::ProgressStage::Authenticating});
    GdtfShareClient catalogClient;
    const GdtfShareResult login = catalogClient.Login(
        onlineCredentials.username, onlineCredentials.password);
    if (!login.Succeeded()) {
      const auto action = gdtf_share_workflow::DetermineCatalogAccessAction(
          false, gdtf_share_workflow::CredentialAvailability::Complete, login,
          false);
      return {action == gdtf_share_workflow::CatalogAccessAction::RequestCredentials
                  ? OnlineStatus::AuthenticationRejected
                  : OnlineStatus::Unavailable,
              std::nullopt, FormatGdtfShareUserMessage(login, "login")};
    }
    if (stopToken.stop_requested())
      return {OnlineStatus::Unavailable, std::nullopt, "cancelled"};
    report({rider_fixture_resolution::ProgressStage::DownloadingCatalog});
    const auto result = catalogService.RefreshCatalogIfStale(
        [&](std::string &payload) {
          if (stopToken.stop_requested())
            return false;
          const GdtfShareResult online = catalogClient.GetCatalog();
          payload = online.payload;
          if (online.Succeeded() && !payload.empty())
            report({rider_fixture_resolution::ProgressStage::ParsingCatalog});
          return online.Succeeded() && !payload.empty();
        },
        catalogRefreshTime, 0);
    if (!result.snapshot)
      return {OnlineStatus::Unavailable, std::nullopt,
              result.failureMessage};
    if (!result.parsedCatalog || !result.parsedCatalog->IsUsable())
      return {OnlineStatus::Unavailable, std::nullopt,
              "catalog payload is unusable"};
    const auto resolvedAction =
        gdtf_share_workflow::DetermineCatalogAccessAction(
            result.source == GdtfCatalogResultSource::Cache,
            gdtf_share_workflow::CredentialAvailability::Complete, login,
            result.source == GdtfCatalogResultSource::Online);
    if (resolvedAction !=
            gdtf_share_workflow::CatalogAccessAction::OpenOnlineCatalog &&
        resolvedAction !=
            gdtf_share_workflow::CatalogAccessAction::OpenCachedCatalog)
      return {OnlineStatus::Unavailable, std::nullopt,
              "catalog access policy rejected the result"};
    RiderFixtureResolutionDialog::CatalogData data{
        *result.snapshot, result.parsedCatalog->entries,
        result.source == GdtfCatalogResultSource::Online
            ? RiderFixtureResolutionDialog::CatalogSource::Online
            : RiderFixtureResolutionDialog::CatalogSource::Cached};
    return {OnlineStatus::Success, std::move(data), {}};
  };

  auto requestCatalogCredentials =
      [&](bool rejected) -> std::optional<CredentialStore::Credentials> {
    (void)rejected;
    const std::string initialUser =
        credentials ? credentials->username
                    : loadedCredentials.usernameHint.value_or(std::string());
    GdtfLoginDialog login(parent, initialUser, std::string());
    if (login.ShowModal() != wxID_OK)
      return std::nullopt;
    CredentialStore::Credentials entered{login.GetUsername(),
                                         login.GetPassword()};
    if (entered.username.empty() || entered.password.empty())
      return std::nullopt;
    credentials = entered;
    return entered;
  };

  auto persistCatalogCredentials =
      [&](const CredentialStore::Credentials &authenticatedCredentials) {
    credentials = authenticatedCredentials;
    const CredentialStore::Result persisted =
        PersistGdtfCredentialsForGui(authenticatedCredentials, configManager);
    if (!persisted.Succeeded()) {
      wxMessageBox(
          _("GDTF Share authentication succeeded, but the credentials could not be saved securely. You may need to enter them again after restart."),
          _("GDTF Share credentials"), wxOK | wxICON_WARNING, parent);
    }
  };

  const auto dialogVisibleMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - preflightStarted).count();
  diagnostics::DiagnosticLogger::Info(
      "Rider fixture preflight timing: rider_analysis_ms=" +
      std::to_string(riderAnalysisMs) + " dictionary_load_ms=" +
      std::to_string(dictionaryLoadMs) + " dialog_visible_ms=" +
      std::to_string(dialogVisibleMs));
  RiderFixtureResolutionDialog dialog(parent, std::move(analysis), dictionary,
                                      loadCachedCatalog, loadOnlineCatalog,
                                      credentials, requestCatalogCredentials,
                                      persistCatalogCredentials);
  if (dialog.ShowModal() != wxID_OK)
    return PreflightResult::Cancelled;
  analysis = dialog.TakeAnalysis();
  LogAnalysisCounts(analysis);

  struct DownloadedSelection {
    std::filesystem::path path;
    std::vector<std::string> modes;
  };
  std::map<std::string, DownloadedSelection> downloads;
  const std::filesystem::path fixtureDirectory(
      ProjectUtils::GetWritableLibraryPath("fixtures"));
  std::error_code directoryError;
  std::filesystem::create_directories(fixtureDirectory, directoryError);
  size_t recoverableFailureCount = 0;
  bool authenticationUnavailable = false;

  for (auto &item : analysis.items) {
    if (!item.create)
      continue;
    if (!item.selectedEntry)
      continue;
    const std::string rid = item.selectedEntry->rid;
    if (downloads.find(rid) != downloads.end())
      continue;
    const std::filesystem::path destination =
        gdtf_download_filename::ChooseDestination(
            fixtureDirectory, item.selectedEntry->manufacturer,
            item.selectedEntry->fixtureName, rid);
    if (std::filesystem::exists(destination)) {
      auto existingModes = GetGdtfModes(destination.string());
      if (!existingModes.empty()) {
        downloads.emplace(
            rid, DownloadedSelection{destination, std::move(existingModes)});
        diagnostics::DiagnosticLogger::Info(
            "Rider fixture GDTF reused locally: revision=" + rid);
        continue;
      }
    }
    if (authenticationUnavailable ||
        !EnsureAuthenticated(parent, configManager, client, credentials)) {
      authenticationUnavailable = true;
      rider_fixture_resolution::Service::FallbackAfterFailure(
          item, "Authentication unavailable");
      ++recoverableFailureCount;
      diagnostics::DiagnosticLogger::Warning(
          "Rider fixture resolution fallback: alias=" + item.request.typeName +
          " effective_type=" + item.effectiveFixtureType +
          " rid=" + rid + " stage=authenticate");
      continue;
    }
    const CredentialStore::Credentials downloadCredentials = *credentials;
    GdtfShareResult download = WaitForNetworkTask(
        parent, _("Resolve fixture types"), _("Downloading selected GDTF..."),
        std::async(std::launch::async,
                   [&client, &rid, &destination, downloadCredentials]() {
          return gdtf_download_workflow::DownloadWithExpiredSessionRetry(
              client, rid, destination.string(), [&]() {
                client.ResetSession();
                return client.Login(downloadCredentials.username,
                                    downloadCredentials.password)
                    .Succeeded();
              });
        }));
    if (!download.Succeeded() || !std::filesystem::exists(destination)) {
      diagnostics::DiagnosticLogger::Error(
          "Rider fixture GDTF download failed: alias=" +
          item.request.typeName + " revision=" + rid);
      rider_fixture_resolution::Service::FallbackAfterFailure(
          item, "Download failed");
      ++recoverableFailureCount;
      continue;
    }
    auto modes = GetGdtfModes(destination.string());
    if (modes.empty()) {
      rider_fixture_resolution::Service::FallbackAfterFailure(
          item, "Downloaded GDTF is invalid");
      ++recoverableFailureCount;
      diagnostics::DiagnosticLogger::Warning(
          "Rider fixture resolution fallback: alias=" + item.request.typeName +
          " effective_type=" + item.effectiveFixtureType + " rid=" + rid +
          " source=" + destination.filename().string() +
          " source_valid=false stage=validate-gdtf");
      continue;
    }
    downloads.emplace(rid, DownloadedSelection{destination, std::move(modes)});
    diagnostics::DiagnosticLogger::Info(
        "Rider fixture GDTF download completed: revision=" + rid);
  }

  for (auto &item : analysis.items) {
    if (!item.create)
      continue;
    if (!item.selectedEntry)
      continue;
    const auto found = downloads.find(item.selectedEntry->rid);
    if (found == downloads.end()) {
      rider_fixture_resolution::Service::FallbackAfterFailure(
          item, "Selected GDTF is unavailable");
      ++recoverableFailureCount;
      continue;
    }
    if (std::find(found->second.modes.begin(), found->second.modes.end(),
                  item.selectedMode) == found->second.modes.end()) {
      const std::string failedRid = item.selectedEntry->rid;
      const std::string failedMode = item.selectedMode;
      rider_fixture_resolution::Service::FallbackAfterFailure(
          item, "Selected mode is not present in the GDTF");
      ++recoverableFailureCount;
      diagnostics::DiagnosticLogger::Warning(
          "Rider fixture resolution fallback: alias=" + item.request.typeName +
          " effective_type=" + item.effectiveFixtureType +
          " rid=" + failedRid + " mode=" + failedMode +
          " stage=validate-mode");
      continue;
    }
  }

  for (auto &item : analysis.items) {
    if (!item.create)
      continue;
    if (item.state != rider_fixture_resolution::State::Dictionary ||
        !item.dictionaryEntry ||
        item.selectedMode == item.dictionaryEntry->mode)
      continue;
    GdtfDictionary::Entry changed = *item.dictionaryEntry;
    changed.mode = item.selectedMode;
    GdtfDictionary::UpdateDictionaryEntry(item.request.typeName, changed);
    const auto persisted = GdtfDictionary::Get(item.request.typeName);
    if (!persisted || persisted->mode != item.selectedMode) {
      item.selectedMode = item.originalDictionaryMode;
      item.origin = rider_fixture_resolution::ResolutionOrigin::Dictionary;
      item.details = "Dictionary mode change could not be saved; original mode retained";
      ++recoverableFailureCount;
      diagnostics::DiagnosticLogger::Warning(
          "Rider fixture dictionary mode fallback: alias=" +
          item.request.typeName + " effective_type=" + item.effectiveFixtureType +
          " mode=" + item.selectedMode + " stage=save-dictionary-mode");
    }
  }

  for (auto &item : analysis.items) {
    if (!item.create)
      continue;
    if (!item.selectedEntry)
      continue;
    const auto downloadedIt = downloads.find(item.selectedEntry->rid);
    if (downloadedIt == downloads.end()) {
      rider_fixture_resolution::Service::FallbackAfterFailure(
          item, "Selected GDTF is unavailable");
      ++recoverableFailureCount;
      continue;
    }
    const auto &downloaded = downloadedIt->second;
    const auto persisted = GdtfDictionary::CreateOrUpdateExternalLibraryMapping(
        item.request.typeName, downloaded.path.string(), item.selectedMode);
    if (!persisted.success) {
      diagnostics::DiagnosticLogger::Warning(
          "Rider fixture resolution fallback: alias=" + item.request.typeName +
          " effective_type=" + item.effectiveFixtureType +
          " rid=" + item.selectedEntry->rid + " mode=" + item.selectedMode +
          " source=" + downloaded.path.filename().string() +
          " source_valid=true derivative_attempted=false stage=" +
          persisted.failureStage + " reason=" + persisted.error);
      rider_fixture_resolution::Service::FallbackAfterFailure(
          item, "Dictionary mapping could not be saved");
      ++recoverableFailureCount;
      continue;
    }
    diagnostics::DiagnosticLogger::Info(
        "Rider fixture mapping accepted: alias=" + item.request.typeName +
        " revision=" + item.selectedEntry->rid + " mode=" + item.selectedMode);
  }
  if (importPlanOut) {
    importPlanOut->fixtureSelections.clear();
    for (const auto &item : analysis.items) {
      importPlanOut->fixtureSelections.push_back({
          item.request.normalizedTypeName, item.effectiveFixtureType,
          item.create});
    }
  }
  if (recoverableFailureCount > 0) {
    wxMessageBox(
        wxString::Format(
            _("%zu fixture types used generic fallback because GDTF resolution failed. See the diagnostic log for details."),
            recoverableFailureCount),
        _("Fixture resolution warning"), wxOK | wxICON_WARNING, parent);
  }
  return PreflightResult::Proceed;
}

} // namespace rider_fixture_resolution_gui
