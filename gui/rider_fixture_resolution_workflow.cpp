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
#include <wx/choicdlg.h>
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
  if (!credentials || credentials->username.empty() || credentials->password.empty()) {
    const auto loaded = LoadGdtfCredentialsForGuiDetailed(configManager);
    credentials = loaded.credentials;
    const std::string initialUser = credentials
        ? credentials->username
        : loaded.usernameHint.value_or(std::string());
    GdtfLoginDialog login(parent, initialUser, std::string());
    if (login.ShowModal() != wxID_OK)
      return false;
    credentials = CredentialStore::Credentials{login.GetUsername(),
                                                login.GetPassword()};
  }
  if (!credentials || credentials->username.empty() || credentials->password.empty())
    return false;
  client.ResetSession();
  const auto result = WaitForNetworkTask(
      parent, _("GDTF Share"), _("Signing in to GDTF Share..."),
      std::async(std::launch::async, [&client, &credentials]() {
        return client.Login(credentials->username, credentials->password);
      }));
  if (!result.Succeeded()) {
    wxMessageBox(wxString::FromUTF8(FormatGdtfShareUserMessage(result, "login")),
                 _("GDTF Share sign-in failed"), wxOK | wxICON_ERROR, parent);
    credentials.reset();
    return false;
  }
  (void)PersistGdtfCredentialsForGui(*credentials, configManager);
  return true;
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
                                           const std::string &text) {
  const auto requests = RiderImporter::AnalyzeFixtureTypes(text);
  const auto dictionary = GdtfDictionary::Load().value_or(
      std::unordered_map<std::string, GdtfDictionary::Entry>{});
  auto analysis = rider_fixture_resolution::Service::Analyze(requests, dictionary, {});
  LogAnalysisCounts(analysis);
  if (!analysis.RequiresPreflight())
    return PreflightResult::Proceed;

  GdtfCatalogService catalogService;
  auto snapshot = catalogService.GetCatalogSnapshot();
  const auto parsedCatalog = snapshot
      ? mvr::gdtf_catalog_parser::ParseCatalog(snapshot->listData).entries
      : std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry>{};
  analysis = rider_fixture_resolution::Service::Analyze(requests, dictionary,
                                                         parsedCatalog);
  LogAnalysisCounts(analysis);

  GdtfShareClient client;
  std::optional<CredentialStore::Credentials> credentials;
  auto loadOnlineCatalog = [&]() -> std::optional<GdtfCatalogSnapshot> {
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
    return result.snapshot;
  };

  RiderFixtureResolutionDialog dialog(
      parent, std::move(analysis), snapshot ? snapshot->listData : std::string{},
      snapshot ? snapshot->updatedAt : std::string{}, loadOnlineCatalog);
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

  for (auto &item : analysis.items) {
    if (!item.selectedEntry)
      continue;
    const auto found = downloads.find(item.selectedEntry->rid);
    if (found == downloads.end())
      return PreflightResult::Failed;
    if (item.selectedMode.empty() && found->second.modes.size() == 1)
      item.selectedMode = found->second.modes.front();
    if (item.selectedMode.empty() && found->second.modes.size() > 1) {
      wxArrayString choices;
      for (const std::string &mode : found->second.modes)
        choices.Add(wxString::FromUTF8(mode));
      wxSingleChoiceDialog modeDialog(
          parent,
          wxString::Format(_("Select a GDTF mode for %s."),
                           wxString::FromUTF8(item.request.typeName)),
          _("Select fixture mode"), choices);
      if (modeDialog.ShowModal() != wxID_OK)
        return PreflightResult::Cancelled;
      item.selectedMode = modeDialog.GetStringSelection().ToStdString();
    }
    if (std::find(found->second.modes.begin(), found->second.modes.end(),
                  item.selectedMode) == found->second.modes.end()) {
      wxMessageBox(wxString::Format(_("The selected mode for %s is not present in the downloaded GDTF. No scene objects were created."),
                                    wxString::FromUTF8(item.request.typeName)),
                   _("Fixture resolution failed"), wxOK | wxICON_ERROR, parent);
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
