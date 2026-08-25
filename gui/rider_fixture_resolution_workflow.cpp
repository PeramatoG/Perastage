#include "rider_fixture_resolution_workflow.h"

#include "rider_fixture_resolution_dialog.h"
#include "logindialog.h"
#include "mainwindow_gdtf_credentials.h"
#include "../core/diagnostics/DiagnosticLogger.h"
#include "../core/gdtf_catalog_service.h"
#include "../core/gdtfdictionary.h"
#include "../core/gdtf_download_workflow.h"
#include "../core/gdtfnet.h"
#include "../core/projectutils.h"
#include "../core/riderimporter.h"
#include "../mvr/gdtf_catalog_parser.h"
#include "gdtfloader.h"

#include <algorithm>
#include <cctype>
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

// Replaces unsafe filename characters in a catalog revision identifier.
std::string SafeFileStem(std::string value) {
  for (char &character : value) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (!std::isalnum(byte) && character != '-' && character != '_')
      character = '_';
  }
  return value.empty() ? "gdtf-share-fixture" : value;
}

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
                 _("GDTF Share sign-in failed"), wxOK | wxICON_ERROR, parent);
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
                                           std::string *filteredTextOut) {
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
  std::optional<CredentialStore::Credentials> credentials;
  auto loadOnlineCatalog = [&]()
      -> std::optional<RiderFixtureResolutionDialog::CatalogData> {
    if (!EnsureAuthenticated(parent, configManager, client, credentials))
      return std::nullopt;
    const auto result = catalogService.RefreshCatalogIfStale(
        [&](std::string &payload) {
          const GdtfShareResult online = WaitForNetworkTask(
              parent, _("GDTF Share"), _("Refreshing the GDTF catalog..."),
              std::async(std::launch::async,
                         [&client]() { return client.GetCatalog(); }));
          payload = online.payload;
          return online.Succeeded() && !payload.empty();
        },
        wxDateTime::UNow().FormatISOCombined(' ').ToStdString(), 0);
    if (!result.snapshot)
      return std::nullopt;
    if (!result.parsedCatalog || !result.parsedCatalog->IsUsable())
      return std::nullopt;
    RiderFixtureResolutionDialog::CatalogData data{
        *result.snapshot, result.parsedCatalog->entries,
        result.source == GdtfCatalogResultSource::Online
            ? RiderFixtureResolutionDialog::CatalogSource::Online
            : RiderFixtureResolutionDialog::CatalogSource::Cached};
    const auto matchStarted = std::chrono::steady_clock::now();
    data.matches = WaitForNetworkTask(
        parent, _("GDTF catalog"), _("Matching rider fixture types..."),
        std::async(std::launch::async, [&requests, &data]() {
          return rider_fixture_resolution::Service::Analyze(requests, {},
                                                             data.entries);
        }));
    data.matchMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - matchStarted).count();
    return data;
  };

  const auto dialogVisibleMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - preflightStarted).count();
  diagnostics::DiagnosticLogger::Info(
      "Rider fixture preflight timing: rider_analysis_ms=" +
      std::to_string(riderAnalysisMs) + " dictionary_load_ms=" +
      std::to_string(dictionaryLoadMs) + " dialog_visible_ms=" +
      std::to_string(dialogVisibleMs));
  RiderFixtureResolutionDialog dialog(parent, std::move(analysis),
                                      loadCachedCatalog, loadOnlineCatalog);
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
  auto rollbackDictionary = [&]() {
    std::string rollbackError;
    if (!GdtfDictionary::Save(dictionary, &rollbackError)) {
      diagnostics::DiagnosticLogger::Error(
          "Rider fixture dictionary rollback failed: " + rollbackError);
    }
  };

  for (const auto &item : analysis.items) {
    if (!item.selectedEntry)
      continue;
    const std::string rid = item.selectedEntry->rid;
    if (downloads.find(rid) != downloads.end())
      continue;
    const std::filesystem::path destination =
        fixtureDirectory / (SafeFileStem(rid) + ".gdtf");
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
    if (!EnsureAuthenticated(parent, configManager, client, credentials))
      return PreflightResult::Failed;
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
      wxMessageBox(wxString::Format(_("Could not download the GDTF selected for %s. No scene objects were created."),
                                    wxString::FromUTF8(item.request.typeName)),
                   _("Fixture resolution failed"), wxOK | wxICON_ERROR, parent);
      return PreflightResult::Failed;
    }
    auto modes = GetGdtfModes(destination.string());
    if (modes.empty()) {
      wxMessageBox(_("The downloaded file does not contain a usable GDTF mode. No scene objects were created."),
                   _("Fixture resolution failed"), wxOK | wxICON_ERROR, parent);
      return PreflightResult::Failed;
    }
    downloads.emplace(rid, DownloadedSelection{destination, std::move(modes)});
    diagnostics::DiagnosticLogger::Info(
        "Rider fixture GDTF download completed: revision=" + rid);
  }

  for (const auto &item : analysis.items) {
    if (!item.selectedEntry)
      continue;
    const auto found = downloads.find(item.selectedEntry->rid);
    if (found == downloads.end())
      return PreflightResult::Failed;
    if (std::find(found->second.modes.begin(), found->second.modes.end(),
                  item.selectedMode) == found->second.modes.end()) {
      wxMessageBox(wxString::Format(_("The selected mode for %s is not present in the downloaded GDTF. No scene objects were created."),
                                    wxString::FromUTF8(item.request.typeName)),
                   _("Fixture resolution failed"), wxOK | wxICON_ERROR, parent);
      return PreflightResult::Failed;
    }
  }

  for (const auto &item : analysis.items) {
    if (item.state != rider_fixture_resolution::State::Dictionary ||
        !item.dictionaryEntry ||
        item.selectedMode == item.dictionaryEntry->mode)
      continue;
    GdtfDictionary::Entry changed = *item.dictionaryEntry;
    changed.mode = item.selectedMode;
    GdtfDictionary::UpdateDictionaryEntry(item.request.typeName, changed);
    const auto persisted = GdtfDictionary::Get(item.request.typeName);
    if (!persisted || persisted->mode != item.selectedMode) {
      rollbackDictionary();
      return PreflightResult::Failed;
    }
  }

  for (const auto &item : analysis.items) {
    if (!item.selectedEntry)
      continue;
    const auto &downloaded = downloads.at(item.selectedEntry->rid);
    const auto persisted = GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
        item.request.typeName, downloaded.path.string(), item.selectedMode);
    if (!persisted) {
      rollbackDictionary();
      wxMessageBox(wxString::Format(_("Could not save the fixture dictionary mapping for %s. No scene objects were created."),
                                    wxString::FromUTF8(item.request.typeName)),
                   _("Fixture resolution failed"), wxOK | wxICON_ERROR, parent);
      return PreflightResult::Failed;
    }
    diagnostics::DiagnosticLogger::Info(
        "Rider fixture mapping accepted: alias=" + item.request.typeName +
        " revision=" + item.selectedEntry->rid + " mode=" + item.selectedMode);
  }
  return PreflightResult::Proceed;
}

} // namespace rider_fixture_resolution_gui
