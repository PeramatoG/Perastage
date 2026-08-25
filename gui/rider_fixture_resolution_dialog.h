#pragma once

#include "../core/rider_fixture_resolution.h"
#include "../core/gdtf_catalog_service.h"

#include <string>
#include <functional>
#include <optional>

#include <wx/dialog.h>

class wxButton;
class wxDataViewEvent;
class wxDataViewListCtrl;
class wxStaticText;
class wxShowEvent;
class wxThreadEvent;

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
  using CatalogLoader = std::function<std::optional<CatalogData>()>;
  RiderFixtureResolutionDialog(
      wxWindow *parent, rider_fixture_resolution::Analysis analysis,
      CatalogLoader cachedCatalogLoader, CatalogLoader onlineCatalogLoader);

  rider_fixture_resolution::Analysis TakeAnalysis();

private:
  void BuildLayout();
  void RefreshTable();
  void RefreshSelectionControls();
  void RefreshSummary();
  void OnSelectionChanged(wxDataViewEvent &event);
  void OnItemActivated(wxDataViewEvent &event);
  void OnUseSuggested(wxCommandEvent &event);
  void OnSearch(wxCommandEvent &event);
  void OnUseGeneric(wxCommandEvent &event);
  void OnAcceptAll(wxCommandEvent &event);
  void OnResolve(wxCommandEvent &event);
  void OnDialogShown(wxShowEvent &event);
  void OnCatalogLoaded(wxThreadEvent &event);
  void ApplyCatalog(const CatalogData &catalog);
  std::vector<RiderImporter::FixtureTypeRequest> BuildFixtureRequests() const;
  rider_fixture_resolution::Item *SelectedItem();

  rider_fixture_resolution::Analysis analysis;
  std::string catalogPayload;
  std::string catalogUpdatedAt;
  std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> catalogEntries;
  CatalogSource catalogSource = CatalogSource::None;
  CatalogLoader cachedCatalogLoader;
  CatalogLoader onlineCatalogLoader;
  bool catalogLoadStarted = false;
  bool catalogLoading = false;
  wxDataViewListCtrl *table = nullptr;
  wxButton *useSuggestedButton = nullptr;
  wxButton *searchButton = nullptr;
  wxButton *useGenericButton = nullptr;
  wxButton *resolveButton = nullptr;
  wxStaticText *summaryLabel = nullptr;
  wxStaticText *catalogStatusLabel = nullptr;
};
