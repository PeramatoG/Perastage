#pragma once

#include "../core/rider_fixture_resolution.h"
#include "../core/gdtf_catalog_service.h"
#include "../core/credentialstore.h"

#include <string>
#include <functional>
#include <optional>
#include <atomic>
#include <thread>

#include <wx/dialog.h>

class wxButton;
class wxGauge;
class wxDataViewEvent;
class wxDataViewCtrl;
class wxDataViewItem;
class wxStaticText;
class wxShowEvent;
class wxThreadEvent;
class RiderFixtureResolutionModel;

class RiderFixtureResolutionDialog final : public wxDialog {
public:
  enum class CatalogSource { None, Cached, Online };
  struct CatalogData {
    GdtfCatalogSnapshot snapshot;
    std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> entries;
    CatalogSource source = CatalogSource::None;
    std::optional<rider_fixture_resolution::Analysis> matches;
    long long matchMs = 0;
  };
  struct ProgressData {
    rider_fixture_resolution::Progress progress;
    size_t row = 0;
    std::optional<rider_fixture_resolution::Item> matchedItem;
  };
  enum class OnlineCatalogStatus {
    Success,
    AuthenticationRejected,
    Unavailable
  };
  struct OnlineCatalogResult {
    OnlineCatalogStatus status = OnlineCatalogStatus::Unavailable;
    std::optional<CatalogData> catalog;
    std::string error;
  };
  using CatalogLoader = std::function<std::optional<CatalogData>()>;
  using OnlineProgressCallback =
      std::function<void(const rider_fixture_resolution::Progress &)>;
  using OnlineCatalogLoader = std::function<OnlineCatalogResult(
      const CredentialStore::Credentials &, std::stop_token,
      OnlineProgressCallback)>;
  using CredentialRequester = std::function<
      std::optional<CredentialStore::Credentials>(bool rejected)>;
  using CredentialPersistCallback =
      std::function<void(const CredentialStore::Credentials &)>;
  RiderFixtureResolutionDialog(
      wxWindow *parent, rider_fixture_resolution::Analysis analysis,
      std::unordered_map<std::string, GdtfDictionary::Entry> dictionary,
      CatalogLoader cachedCatalogLoader, OnlineCatalogLoader onlineCatalogLoader,
      std::optional<CredentialStore::Credentials> initialCredentials,
      CredentialRequester credentialRequester,
      CredentialPersistCallback credentialPersistCallback);
  ~RiderFixtureResolutionDialog() override;

  rider_fixture_resolution::Analysis TakeAnalysis();

private:
  void BuildLayout();
  void PopulateTable();
  void UpdateRow(size_t analysisIndex);
  void RefreshSelectionControls();
  void RefreshSummary();
  void RefreshCatalogCompletionStatus();
  void OnSelectionChanged(wxDataViewEvent &event);
  void OnItemActivated(wxDataViewEvent &event);
  void OnValueChanged(wxDataViewEvent &event);
  void OnUseSuggested(wxCommandEvent &event);
  void OnSearch(wxCommandEvent &event);
  void OnUseGeneric(wxCommandEvent &event);
  void OnAcceptAll(wxCommandEvent &event);
  void OnResolve(wxCommandEvent &event);
  void OnCancel(wxCommandEvent &event);
  void OnDialogShown(wxShowEvent &event);
  void OnCatalogLoaded(wxThreadEvent &event);
  void OnOnlineCatalogLoaded(wxThreadEvent &event);
  void OnProgress(wxThreadEvent &event);
  void ApplyCatalog(const CatalogData &catalog);
  std::vector<RiderImporter::FixtureTypeRequest> BuildFixtureRequests() const;
  rider_fixture_resolution::Item *SelectedItem();
  std::optional<size_t> AnalysisIndexForItem(const wxDataViewItem &item) const;
  std::optional<unsigned> StoreRowForAnalysisIndex(size_t analysisIndex) const;
  void RequestWorkerStop();
  void BeginOnlineCatalogAcquisition(bool rejectedCredentials = false);
  void StartOnlineCatalogWorker(const CredentialStore::Credentials &credentials);

  rider_fixture_resolution::Analysis analysis;
  std::unordered_map<std::string, GdtfDictionary::Entry> dictionary;
  std::string catalogPayload;
  std::string catalogUpdatedAt;
  std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> catalogEntries;
  CatalogSource catalogSource = CatalogSource::None;
  CatalogLoader cachedCatalogLoader;
  OnlineCatalogLoader onlineCatalogLoader;
  std::optional<CredentialStore::Credentials> catalogCredentials;
  CredentialRequester credentialRequester;
  CredentialPersistCallback credentialPersistCallback;
  bool catalogLoadStarted = false;
  bool catalogLoading = false;
  bool onlineCatalogLoadAttempted = false;
  bool acceptAutomaticResults = true;
  bool modelUpdateInProgress = false;
  std::atomic<bool> shuttingDown{false};
  std::jthread catalogWorker;
  wxDataViewCtrl *table = nullptr;
  RiderFixtureResolutionModel *tableModel = nullptr;
  wxButton *useSuggestedButton = nullptr;
  wxButton *searchButton = nullptr;
  wxButton *useGenericButton = nullptr;
  wxButton *resolveButton = nullptr;
  wxButton *acceptAllButton = nullptr;
  wxGauge *catalogProgressGauge = nullptr;
  wxStaticText *summaryLabel = nullptr;
  wxStaticText *catalogStatusLabel = nullptr;
};
