/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
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
#pragma once
#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/timer.h>
#include <functional>
#include <optional>
#include <thread>
#include <vector>
#include "../mvr/gdtf_catalog_matcher.h"
#include "../mvr/gdtf_catalog_parser.h"

enum class GdtfCatalogDisplaySource {
    None,
    Cached,
    Online
};

class GdtfSearchDialog : public wxDialog {
public:
    struct RefreshResult {
        bool success = false;
        std::string listData;
        std::string updatedAt;
        std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> parsedEntries;
        std::string payloadFingerprint;
        std::string failureDetails;
        long long refreshMs = 0;
        long long parseMs = 0;
        GdtfCatalogDisplaySource source = GdtfCatalogDisplaySource::None;
    };
    using RefreshCatalogFn = std::function<RefreshResult()>;

    GdtfSearchDialog(wxWindow* parent, const std::string& listData,
                     const std::string& cachedUpdatedAt,
                     RefreshCatalogFn refreshCatalogFn,
                     GdtfCatalogDisplaySource initialSource = GdtfCatalogDisplaySource::Cached,
                     bool downloadRequiresAuthentication = false,
                     const std::string& initialFixtureQuery = {},
                     std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry>
                         initialParsedEntries = {});
    ~GdtfSearchDialog() override;
    std::string GetSelectedId() const;
    std::string GetSelectedUrl() const;
    std::string GetSelectedName() const;
    std::optional<mvr::gdtf_catalog_matcher::GdtfCatalogEntry>
    GetSelectedEntry() const;
    std::string GetCurrentListData() const;
private:
    void ParseList(const std::string& listData);
    void UpdateResults();
    void RenderCurrentPage(const std::string& previouslySelectedRid);
    void UpdatePaginationControls();
    void OnSearch(wxCommandEvent& evt);
    void OnSearchTextChanged(wxCommandEvent& evt);
    void OnSearchDebounceTimer(wxTimerEvent& evt);
    void OnPrevPage(wxCommandEvent& evt);
    void OnNextPage(wxCommandEvent& evt);
    void OnDownload(wxCommandEvent& evt);
    void OnDialogShown(wxShowEvent& evt);
    void TriggerAutoRefreshOnce();
    void OnAutoRefreshThreadEvent(wxThreadEvent& evt);
    void OnAutoRefreshFinished(const RefreshResult& result);
    bool FinishRefreshBeforeModalClose();
    void UpdateStatusMessage(bool refreshing, const wxString& details = {});
    void MaybeLogVerboseCatalogTrace(const wxString& message) const;

    wxTextCtrl* manufacturerCtrl = nullptr;
    wxTextCtrl* fixtureCtrl = nullptr;
    wxTextCtrl* generalQueryCtrl = nullptr;
    wxDataViewListCtrl* resultTable = nullptr;
    wxStaticText* statusLabel = nullptr;
    wxButton* prevPageButton = nullptr;
    wxButton* nextPageButton = nullptr;
    wxStaticText* pageInfoLabel = nullptr;
    wxButton* downloadButton = nullptr;
    std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> entries;
    std::vector<int> filteredIndices;
    std::vector<int> visible;
    size_t currentPage = 0;
    size_t pageSize = 500;
    int selectedIndex = -1;
    std::string currentListData;
    std::string lastUpdatedAt;
    RefreshCatalogFn refreshCatalogFn;
    GdtfCatalogDisplaySource catalogSource = GdtfCatalogDisplaySource::None;
    bool downloadRequiresAuthentication = false;
    bool autoRefreshInProgress = false;
    bool autoRefreshTriggered = false;
    std::thread autoRefreshThread;
    wxTimer searchDebounceTimer;
    long long lastParseMs = 0;
    long long lastFilterMs = 0;
    long long lastRenderMs = 0;
};
