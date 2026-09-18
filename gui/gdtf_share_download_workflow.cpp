#include "gdtf_share_download_workflow.h"

#include "mainwindow_menu_text_utils.h"
#include "gdtf_share_message_formatter.h"
#include "gdtfsearchdialog.h"
#include "logindialog.h"
#include "mainwindow_gdtf_credentials.h"

#include "credentialstore.h"
#include "diagnostics/DiagnosticLogger.h"
#include "gdtf_catalog_service.h"
#include "gdtf_download_filename.h"
#include "gdtf_download_workflow.h"
#include "gdtf_share_workflow.h"
#include "gdtfnet.h"
#include "projectutils.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <wx/busyinfo.h>
#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>
#include <wx/window.h>

// Runs the complete GDTF Share catalog, search, and download GUI workflow.
void RunGdtfShareDownloadWorkflow(
    wxWindow *parent, ConfigManager &configManager,
    const GdtfShareDownloadGuiCallbacks &callbacks) {
  diagnostics::DiagnosticLogger::Info("GDTF download workflow opened.");
  CredentialStore::LoadResult loadedCredentials =
      LoadGdtfCredentialsForGuiDetailed(configManager);
  std::optional<CredentialStore::Credentials> activeCredentials =
      loadedCredentials.credentials;

  GdtfShareClient gdtfClient;
  gdtf_share_workflow::WorkflowState gdtfWorkflowState;
  gdtfWorkflowState.credentialAvailability =
      gdtf_share_workflow::DetermineCredentialAvailability(loadedCredentials);

  const auto catalogResolveStart = std::chrono::steady_clock::now();
  GdtfCatalogService catalogService;
  const std::string nowUtc =
      WxToUtf8(wxDateTime::UNow().FormatISOCombined(' '));

  std::unique_ptr<wxWindowDisabler> refreshDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> refreshOverlay =
      std::make_unique<wxBusyInfo>("Updating GDTF catalog...");
  wxYieldIfNeeded();

  GdtfShareResult initialLoginResult;
  GdtfShareResult initialCatalogResult;
  bool initialLoginAttempted = false;
  GdtfCatalogRefreshResult catalogResult =
      catalogService.RefreshCatalogIfStale(
          [&](std::string &onlineListData) {
            if (!activeCredentials || activeCredentials->username.empty() ||
                activeCredentials->password.empty()) {
              return false;
            }

            initialLoginAttempted = true;
            initialLoginResult = gdtfClient.Login(activeCredentials->username,
                                                  activeCredentials->password);
            gdtfWorkflowState.lastAuthenticationResult = initialLoginResult;
            if (!initialLoginResult.Succeeded())
              return false;
            gdtfWorkflowState.sessionAuthenticated = true;
            gdtfWorkflowState.authenticatedUsername = activeCredentials->username;

            initialCatalogResult = gdtfClient.GetCatalog();
            gdtfWorkflowState.lastCatalogResult = initialCatalogResult;
            onlineListData = initialCatalogResult.payload;
            return initialCatalogResult.Succeeded() && !onlineListData.empty();
          },
          nowUtc, 0);

  refreshOverlay.reset();
  refreshDisabler.reset();

  bool catalogAuthenticationCancelled = false;
  if (!catalogResult.snapshot) {
    auto requestCatalogCredentials = [&]() -> bool {
      while (true) {
        const std::string initialUser =
            activeCredentials
                ? activeCredentials->username
                : loadedCredentials.usernameHint.value_or(std::string());
        GdtfLoginDialog loginDlg(parent, initialUser, std::string());
        if (loginDlg.ShowModal() != wxID_OK) {
          catalogAuthenticationCancelled = true;
          diagnostics::DiagnosticLogger::Info(
              "GDTF catalog authentication cancelled; search dialog not opened.");
          return false;
        }

        CredentialStore::Credentials enteredCredentials;
        enteredCredentials.username = WxToUtf8(
            wxString::FromUTF8(loginDlg.GetUsername()).Trim(true).Trim(false));
        enteredCredentials.password = loginDlg.GetPassword();
        if (enteredCredentials.username.empty() ||
            enteredCredentials.password.empty()) {
          wxMessageBox(_("Please provide username and password."),
                       _("Login Error"), wxOK | wxICON_ERROR, parent);
          continue;
        }
        activeCredentials = std::move(enteredCredentials);
        loadedCredentials.usernameHint = activeCredentials->username;
        return true;
      }
    };

    gdtf_share_workflow::CatalogAccessAction accessAction =
        gdtf_share_workflow::DetermineCatalogAccessAction(
            false, gdtfWorkflowState.credentialAvailability,
            gdtfWorkflowState.lastAuthenticationResult, false);
    while (accessAction ==
           gdtf_share_workflow::CatalogAccessAction::RequestCredentials) {
      diagnostics::DiagnosticLogger::Info(
          "GDTF catalog login requested because no cache is available.");
      activeCredentials.reset();
      if (!requestCatalogCredentials())
        return;

      initialLoginAttempted = true;
      gdtfClient.ResetSession();
      initialLoginResult = gdtfClient.Login(activeCredentials->username,
                                            activeCredentials->password);
      gdtfWorkflowState.lastAuthenticationResult = initialLoginResult;
      gdtfWorkflowState.credentialAvailability =
          gdtf_share_workflow::CredentialAvailability::Complete;
      if (initialLoginResult.Succeeded()) {
        gdtfWorkflowState.sessionAuthenticated = true;
        gdtfWorkflowState.authenticatedUsername = activeCredentials->username;
        const CredentialStore::Result persistResult =
            PersistGdtfCredentialsForGui(*activeCredentials, configManager);
        if (!persistResult.Succeeded()) {
          diagnostics::DiagnosticLogger::Warning(
              std::string("GDTF credentials authenticated but persistence failed: status=") +
              CredentialStore::StatusName(persistResult.status));
          wxMessageBox(
              _("GDTF Share credentials were authenticated for this operation, but the password could not be saved securely. You may need to enter it again after restart."),
              _("GDTF Share credentials"), wxOK | wxICON_WARNING, parent);
        }
        catalogResult = catalogService.RefreshCatalogIfStale(
            [&](std::string &onlineListData) {
              initialCatalogResult = gdtfClient.GetCatalog();
              gdtfWorkflowState.lastCatalogResult = initialCatalogResult;
              onlineListData = initialCatalogResult.payload;
              return initialCatalogResult.Succeeded() &&
                     !onlineListData.empty();
            },
            nowUtc, 0);
      }
      accessAction = gdtf_share_workflow::DetermineCatalogAccessAction(
          false, gdtfWorkflowState.credentialAvailability,
          gdtfWorkflowState.lastAuthenticationResult,
          catalogResult.snapshot.has_value(), catalogAuthenticationCancelled);
    }

    if (!catalogResult.snapshot) {
      std::string detail = catalogResult.failureMessage;
      const std::optional<GdtfShareResult> &shareResult =
          gdtfWorkflowState.lastCatalogResult
              ? gdtfWorkflowState.lastCatalogResult
              : gdtfWorkflowState.lastAuthenticationResult;
      if (shareResult) {
        detail = "result=" + GdtfShareResultCategoryName(shareResult->category) +
                 " http=" + std::to_string(shareResult->httpStatus) +
                 " transport=" + std::to_string(shareResult->transportCode);
      }
      diagnostics::DiagnosticLogger::Error(
          "GDTF online catalog resolution failed; search dialog not opened: " +
          detail);
      wxMessageBox(
          _("The GDTF catalog could not be loaded. Check your Internet connection and GDTF Share credentials."),
          _("GDTF catalog unavailable"), wxOK | wxICON_ERROR, parent);
      return;
    }
  }

  const auto catalogResolveElapsedMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - catalogResolveStart)
          .count();

  std::string effectiveListData = "{}";
  std::string effectiveUpdatedAt = "unknown";
  if (catalogResult.snapshot) {
    effectiveListData = catalogResult.snapshot->listData;
    effectiveUpdatedAt = catalogResult.snapshot->updatedAt;
    gdtfWorkflowState.catalogSource = catalogResult.metrics.refreshSucceeded
        ? gdtf_share_workflow::CatalogSource::Online
        : gdtf_share_workflow::CatalogSource::Cached;
  }
  diagnostics::DiagnosticLogger::Info(
      std::string("GDTF catalog resolved: cache_hit=") +
      (catalogResult.metrics.cacheHit ? "true" : "false") +
      " refresh_attempted=" +
      (catalogResult.metrics.refreshAttempted ? "true" : "false") +
      " refresh_succeeded=" +
      (catalogResult.metrics.refreshSucceeded ? "true" : "false") +
      " credential_state=" +
      gdtf_share_workflow::CredentialAvailabilityName(
          gdtfWorkflowState.credentialAvailability) +
      " catalog_source=" +
      gdtf_share_workflow::CatalogSourceName(gdtfWorkflowState.catalogSource) +
      " session_authenticated=" +
      (gdtfWorkflowState.sessionAuthenticated ? "true" : "false"));

  if (callbacks.appendConsoleMessage) {
    callbacks.appendConsoleMessage(wxString::Format(
        "[METRIC] GDTF catalog cache_hit=%d cache_miss=%d cache_age_s=%lld "
        "refresh_attempted=%d refresh_succeeded=%d resolve_ms=%lld",
        catalogResult.metrics.cacheHit ? 1 : 0,
        catalogResult.metrics.cacheMiss ? 1 : 0,
        static_cast<long long>(catalogResult.metrics.cacheAgeSeconds),
        catalogResult.metrics.refreshAttempted ? 1 : 0,
        catalogResult.metrics.refreshSucceeded ? 1 : 0,
        static_cast<long long>(catalogResolveElapsedMs)));
  }

  std::unique_ptr<wxBusyInfo> preparingCatalogOverlay =
      std::make_unique<wxBusyInfo>("Loading GDTF catalog...");
  wxYieldIfNeeded();
  GdtfSearchDialog searchDlg(
      parent, effectiveListData, effectiveUpdatedAt, nullptr,
      gdtfWorkflowState.catalogSource == gdtf_share_workflow::CatalogSource::Online
          ? GdtfCatalogDisplaySource::Online
          : (gdtfWorkflowState.catalogSource == gdtf_share_workflow::CatalogSource::Cached
                 ? GdtfCatalogDisplaySource::Cached
                 : GdtfCatalogDisplaySource::None),
      !gdtfWorkflowState.sessionAuthenticated);
  preparingCatalogOverlay.reset();

  const int searchDialogResult = searchDlg.ShowModal();

  if (searchDialogResult == wxID_OK) {

    std::unique_ptr<wxWindowDisabler> gdtfDownloadDisabler =
        std::make_unique<wxWindowDisabler>();
    std::unique_ptr<wxBusyInfo> gdtfDownloadBusyOverlay;
    auto clearGdtfDownloadBlockingUi = [&]() {
      gdtfDownloadBusyOverlay.reset();
      gdtfDownloadDisabler.reset();
    };
    auto updateGdtfDownloadBusyOverlay = [&](const wxString &message) {
      gdtfDownloadBusyOverlay = std::make_unique<wxBusyInfo>(message);
      wxYieldIfNeeded();
    };
    auto showGdtfDownloadError = [&](const wxString &message,
                                     const wxString &caption) {
      clearGdtfDownloadBlockingUi();
      wxMessageBox(message, caption, wxOK | wxICON_ERROR, parent);
    };
    auto requestCredentialsFromDialog = [&]() -> bool {
      const std::string initialUser =
          activeCredentials ? activeCredentials->username :
          loadedCredentials.usernameHint.value_or(std::string());
      const std::string initialPassword =
          activeCredentials ? activeCredentials->password : std::string();

      clearGdtfDownloadBlockingUi();
      GdtfLoginDialog loginDlg(parent, initialUser, initialPassword);
      if (loginDlg.ShowModal() != wxID_OK)
        return false;

      gdtfDownloadDisabler = std::make_unique<wxWindowDisabler>();

      CredentialStore::Credentials dialogCredentials;
      dialogCredentials.username = WxToUtf8(
          wxString::FromUTF8(loginDlg.GetUsername()).Trim(true).Trim(false));
      dialogCredentials.password = loginDlg.GetPassword();

      if (dialogCredentials.username.empty() ||
          dialogCredentials.password.empty()) {
        showGdtfDownloadError(_("Please provide username and password."),
                              _("Login Error"));
        return false;
      }

      activeCredentials = dialogCredentials;
      return true;
    };
    auto showPersistenceWarning = [&](const CredentialStore::Result &result) {
      if (result.Succeeded())
        return;
      clearGdtfDownloadBlockingUi();
      wxString message;
      if (result.status == CredentialStore::Status::SecureStoreUnavailable) {
        const wxString detail = wxString::FromUTF8(result.message);
        if (detail.Contains("disabled")) {
          message = _("This build does not include secure password storage. The username was saved, but the password must be entered again after restart.");
        } else {
          message = _("The native secure password store is unavailable. The username was saved, but the password must be entered again after restart.");
        }
      } else if (result.status == CredentialStore::Status::MetadataWriteFailed) {
        message = _("GDTF Share credentials were authenticated for this operation, but the non-secret username metadata could not be saved.");
      } else {
        message = wxString::Format(_("GDTF Share credentials were authenticated for this operation, but the password was not saved (%s)."),
                                   wxString::FromUTF8(CredentialStore::StatusName(result.status)));
      }
      wxMessageBox(message, _("GDTF Share credentials"), wxOK | wxICON_WARNING, parent);
    };
    auto ensureAuthenticated = [&](GdtfShareResult &loginResult) -> bool {
      if (activeCredentials && gdtfClient.IsAuthenticated() &&
          gdtfClient.AuthenticatedUsername() == activeCredentials->username) {
        diagnostics::DiagnosticLogger::Info(
            "GDTF download auth: reused_existing_session=true");
        return true;
      }
      if (!activeCredentials || activeCredentials->username.empty() ||
          activeCredentials->password.empty()) {
        diagnostics::DiagnosticLogger::Info(
            "GDTF download auth prompt reason=missing_credentials");
        if (!requestCredentialsFromDialog())
          return false;
      }
      if (!activeCredentials)
        return false;
      if (initialLoginAttempted &&
          initialLoginResult.category == GdtfShareResultCategory::AuthenticationRejected) {
        diagnostics::DiagnosticLogger::Info(
            "GDTF download auth prompt reason=rejected_credentials");
        if (!requestCredentialsFromDialog())
          return false;
      }
      gdtfClient.ResetSession();
      if (callbacks.appendConsoleMessage)
        callbacks.appendConsoleMessage(
            "[INFO] Logging into GDTF Share using libcurl");
      updateGdtfDownloadBusyOverlay(_("Logging in to GDTF Share..."));
      loginResult = gdtfClient.Login(activeCredentials->username,
                                     activeCredentials->password);
      gdtfWorkflowState.lastAuthenticationResult = loginResult;
      gdtfWorkflowState.sessionAuthenticated = loginResult.Succeeded();
      if (loginResult.Succeeded()) {
        gdtfWorkflowState.authenticatedUsername = activeCredentials->username;
        const CredentialStore::Result persistResult =
            PersistGdtfCredentialsForGui(*activeCredentials, configManager);
        loadedCredentials.usernameHint = activeCredentials->username;
        loadedCredentials.credentials = activeCredentials;
        showPersistenceWarning(persistResult);
      } else if (loginResult.category == GdtfShareResultCategory::AuthenticationRejected) {
        diagnostics::DiagnosticLogger::Info(
            "GDTF download auth prompt reason=rejected_credentials");
        if (!requestCredentialsFromDialog())
          return false;
        gdtfClient.ResetSession();
        loginResult = gdtfClient.Login(activeCredentials->username,
                                       activeCredentials->password);
        gdtfWorkflowState.lastAuthenticationResult = loginResult;
        gdtfWorkflowState.sessionAuthenticated = loginResult.Succeeded();
        if (loginResult.Succeeded()) {
          gdtfWorkflowState.authenticatedUsername = activeCredentials->username;
          const CredentialStore::Result persistResult =
              PersistGdtfCredentialsForGui(*activeCredentials, configManager);
          loadedCredentials.usernameHint = activeCredentials->username;
          loadedCredentials.credentials = activeCredentials;
          showPersistenceWarning(persistResult);
        }
      }
      return loginResult.Succeeded();
    };

    GdtfShareResult loginResult;
    if (!ensureAuthenticated(loginResult)) {
      showGdtfDownloadError(FormatLocalizedGdtfShareUserMessage(
                                loginResult, GdtfShareGuiOperation::Login),
                            _("Login Error"));
      return;
    }

    wxString rid = wxString::FromUTF8(searchDlg.GetSelectedId());
    wxString name = wxString::FromUTF8(searchDlg.GetSelectedName());

    std::string manufacturer;
    std::string fixtureName = WxToUtf8(name);
    if (const auto selectedEntry = searchDlg.GetSelectedEntry()) {
      manufacturer = selectedEntry->manufacturer;
      fixtureName = selectedEntry->fixtureName;
    }

    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
    const wxString suggestedFileName = wxString::FromUTF8(
        gdtf_download_filename::BuildReadableFileName(manufacturer,
                                                      fixtureName));
    wxFileDialog saveDlg(parent, _("Save GDTF file"), fixDir, suggestedFileName,
                         "*.gdtf", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    clearGdtfDownloadBlockingUi();
    if (saveDlg.ShowModal() == wxID_OK) {
      wxString dest = saveDlg.GetPath();
      diagnostics::DiagnosticLogger::Info(
          "GDTF download selected: " +
          diagnostics::DiagnosticLogger::FileNameOnly(WxToUtf8(dest)));
      if (!rid.empty()) {
        gdtfDownloadDisabler = std::make_unique<wxWindowDisabler>();
        updateGdtfDownloadBusyOverlay(_("Downloading GDTF from GDTF Share..."));
        if (callbacks.appendConsoleMessage)
          callbacks.appendConsoleMessage("[INFO] Downloading via libcurl rid=" +
                                      rid);
        GdtfShareResult downloadResult =
            gdtf_download_workflow::DownloadWithExpiredSessionRetry(
                gdtfClient, WxToUtf8(rid), WxToUtf8(dest), [&]() {
                  diagnostics::DiagnosticLogger::Info(
                      "GDTF download auth prompt reason=expired_session");
                  activeCredentials.reset();
                  if (!requestCredentialsFromDialog())
                    return false;
                  gdtfClient.ResetSession();
                  GdtfShareResult retryLogin;
                  return ensureAuthenticated(retryLogin);
                });
        long dlCode = downloadResult.httpStatus;
        bool ok = downloadResult.Succeeded();
        clearGdtfDownloadBlockingUi();
        if (callbacks.appendConsoleMessage)
          callbacks.appendConsoleMessage(
              wxString::Format("[INFO] Download HTTP code: %ld", dlCode));
        if (ok && dlCode == 200) {
          diagnostics::DiagnosticLogger::Info(
              "GDTF download completed: " +
              diagnostics::DiagnosticLogger::FileNameOnly(WxToUtf8(dest)));
          int addNow =
              wxMessageBox(_("GDTF downloaded successfully. Do you want to add "
                           "it to the project now?"),
                           _("Success"), wxYES_NO | wxICON_QUESTION, parent);
          if (addNow == wxYES && callbacks.addFixtureFromGdtfPath)
            callbacks.addFixtureFromGdtfPath(WxToUtf8(dest));
        } else {
          diagnostics::DiagnosticLogger::Error("GDTF download failed: http=" +
                                               std::to_string(dlCode));
          wxMessageBox(FormatLocalizedGdtfShareUserMessage(
                           downloadResult, GdtfShareGuiOperation::Download), _("Error"),
                       wxOK | wxICON_ERROR);
        }
      } else {
        wxMessageBox(_("Download information missing."), _("Error"),
                     wxOK | wxICON_ERROR);
      }
    }
  }

}
