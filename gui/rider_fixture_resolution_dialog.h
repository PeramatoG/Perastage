#pragma once

#include "../core/rider_fixture_resolution.h"
#include "../core/gdtf_catalog_service.h"

#include <string>
#include <functional>
#include <optional>

#include <wx/dialog.h>

class wxButton;
class wxChoice;
class wxDataViewEvent;
class wxDataViewListCtrl;
class wxStaticText;

class RiderFixtureResolutionDialog final : public wxDialog {
public:
  using CatalogLoader = std::function<std::optional<GdtfCatalogSnapshot>()>;
  RiderFixtureResolutionDialog(
      wxWindow *parent, rider_fixture_resolution::Analysis analysis,
      std::string catalogPayload, std::string catalogUpdatedAt,
      CatalogLoader catalogLoader = {});

  rider_fixture_resolution::Analysis TakeAnalysis();

private:
  void BuildLayout();
  void RefreshTable();
  void RefreshSelectionControls();
  void RefreshSummary();
  void OnSelectionChanged(wxDataViewEvent &event);
  void OnUseSuggested(wxCommandEvent &event);
  void OnSearch(wxCommandEvent &event);
  void OnUseGeneric(wxCommandEvent &event);
  void OnAcceptAll(wxCommandEvent &event);
  void OnModeSelected(wxCommandEvent &event);
  void OnResolve(wxCommandEvent &event);
  rider_fixture_resolution::Item *SelectedItem();

  rider_fixture_resolution::Analysis analysis;
  std::string catalogPayload;
  std::string catalogUpdatedAt;
  CatalogLoader catalogLoader;
  wxDataViewListCtrl *table = nullptr;
  wxChoice *modeChoice = nullptr;
  wxButton *useSuggestedButton = nullptr;
  wxButton *searchButton = nullptr;
  wxButton *useGenericButton = nullptr;
  wxButton *resolveButton = nullptr;
  wxStaticText *summaryLabel = nullptr;
};
