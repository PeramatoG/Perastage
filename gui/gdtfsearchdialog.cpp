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
#include "gdtfsearchdialog.h"
#include "columnutils.h"
#include "ui_feature_flags.h"
#include <algorithm>
#include <chrono>
#include <mutex>
#include <wx/intl.h>
#include <wx/datetime.h>
#include <wx/log.h>

wxDEFINE_EVENT(EVT_GDTF_REFRESH_DONE, wxThreadEvent);

namespace {
wxString FormatTimestamp(const std::string& ts);

std::mutex g_cachedCatalogMutex;
std::string g_cachedCatalogPayload;
std::string g_cachedCatalogFingerprint;
std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> g_cachedCatalogEntries;

// Formats parsed mode names for the search result table.
std::string FormatModes(
    const std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogModeCandidate>& modes)
{
    std::string formatted;
    for (const auto& mode : modes) {
        if (!formatted.empty())
            formatted += ", ";
        formatted += mode.name;
    }
    return formatted;
}

wxString FormatTimestamp(const std::string& ts)
{
    if (ts.empty())
        return {};

    try {
        long long val = std::stoll(ts);
        if (val > 1000000000000LL)
            val /= 1000; // milliseconds to seconds
        wxDateTime dt(static_cast<time_t>(val));
        return dt.FormatISOCombined(' ');
    } catch (...) {
        wxDateTime dt;
        if (dt.ParseISOCombined(ts.c_str()))
            return dt.FormatISOCombined(' ');
    }
    return wxString::FromUTF8(ts);
}

constexpr int kSearchDebounceMs = 180;
constexpr size_t kDefaultPageSize = 500;
} // namespace

GdtfSearchDialog::GdtfSearchDialog(wxWindow* parent, const std::string& listData,
                                   const std::string& cachedUpdatedAt,
                                   RefreshCatalogFn refreshCatalogFnIn,
                                   GdtfCatalogDisplaySource initialSource,
                                   bool downloadRequiresAuthenticationIn,
                                   const std::string& initialFixtureQuery,
                                   std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry>
                                       initialParsedEntries)
    : wxDialog(parent, wxID_ANY, _("Search GDTF"), wxDefaultPosition,
               wxSize(1000,700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      currentListData(listData),
      lastUpdatedAt(cachedUpdatedAt),
      refreshCatalogFn(std::move(refreshCatalogFnIn)),
      catalogSource(initialSource),
      downloadRequiresAuthentication(downloadRequiresAuthenticationIn),
      searchDebounceTimer(this)
{
    pageSize = kDefaultPageSize;
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
    searchSizer->Add(new wxStaticText(this, wxID_ANY, _("Manufacturer:")), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);
    manufacturerCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                     wxDefaultSize, wxTE_PROCESS_ENTER);
    searchSizer->Add(manufacturerCtrl, 1, wxRIGHT, 10);
    searchSizer->Add(new wxStaticText(this, wxID_ANY, _("Fixture:")), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);
    fixtureCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                 wxDefaultSize, wxTE_PROCESS_ENTER);
    searchSizer->Add(fixtureCtrl, 1);
    sizer->Add(searchSizer, 0, wxEXPAND | wxALL, 10);

    statusLabel = new wxStaticText(this, wxID_ANY, "");
    sizer->Add(statusLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* pageSizer = new wxBoxSizer(wxHORIZONTAL);
    prevPageButton = new wxButton(this, wxID_ANY, _("< Prev"));
    nextPageButton = new wxButton(this, wxID_ANY, _("Next >"));
    pageInfoLabel = new wxStaticText(this, wxID_ANY, _("Page 1/1"));
    pageSizer->Add(prevPageButton, 0, wxRIGHT, 8);
    pageSizer->Add(nextPageButton, 0, wxRIGHT, 10);
    pageSizer->Add(pageInfoLabel, 0, wxALIGN_CENTER_VERTICAL);
    pageSizer->AddStretchSpacer(1);
    sizer->Add(pageSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    resultTable = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                         wxDefaultSize, wxDV_ROW_LINES);
    int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
    resultTable->AppendTextColumn(_("Manufacturer"), wxDATAVIEW_CELL_INERT, 150,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Fixture"), wxDATAVIEW_CELL_INERT, 200,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Modes"), wxDATAVIEW_CELL_INERT, 60,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Creator"), wxDATAVIEW_CELL_INERT, 120,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Uploader"), wxDATAVIEW_CELL_INERT, 100,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Creation Date"), wxDATAVIEW_CELL_INERT, 110,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Revision"), wxDATAVIEW_CELL_INERT, 90,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Last Modified"), wxDATAVIEW_CELL_INERT, 110,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Version"), wxDATAVIEW_CELL_INERT, 80,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn(_("Rating"), wxDATAVIEW_CELL_INERT, 60,
                                  wxALIGN_LEFT, flags);
    ColumnUtils::EnforceMinColumnWidth(resultTable);
    sizer->Add(resultTable, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    downloadButton = new wxButton(this, wxID_OK, _("Download"));
    wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, _("Cancel"));
    btnSizer->AddStretchSpacer(1);
    btnSizer->Add(downloadButton, 0, wxRIGHT, 5);
    btnSizer->Add(cancelBtn, 0);
    sizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(sizer);
    SetMinSize(wxSize(800, 600));
    SetSize(wxSize(1000, 700));

    manufacturerCtrl->Bind(wxEVT_TEXT_ENTER, &GdtfSearchDialog::OnSearch, this);
    fixtureCtrl->Bind(wxEVT_TEXT_ENTER, &GdtfSearchDialog::OnSearch, this);
    manufacturerCtrl->Bind(wxEVT_TEXT, &GdtfSearchDialog::OnSearchTextChanged, this);
    fixtureCtrl->Bind(wxEVT_TEXT, &GdtfSearchDialog::OnSearchTextChanged, this);
    downloadButton->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnDownload, this);
    prevPageButton->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnPrevPage, this);
    nextPageButton->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnNextPage, this);
    resultTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                      &GdtfSearchDialog::OnDownload, this);
    Bind(wxEVT_SHOW, &GdtfSearchDialog::OnDialogShown, this);
    Bind(EVT_GDTF_REFRESH_DONE, &GdtfSearchDialog::OnAutoRefreshThreadEvent, this);
    Bind(wxEVT_TIMER, &GdtfSearchDialog::OnSearchDebounceTimer, this,
         searchDebounceTimer.GetId());

    if (initialParsedEntries.empty())
        ParseList(currentListData);
    else
        entries = std::move(initialParsedEntries);
    fixtureCtrl->ChangeValue(wxString::FromUTF8(initialFixtureQuery));
    UpdateResults();
    UpdateStatusMessage(false);
}

GdtfSearchDialog::~GdtfSearchDialog()
{
    if (autoRefreshThread.joinable())
        autoRefreshThread.join();
}

void GdtfSearchDialog::ParseList(const std::string& listData)
{
    const auto parseStart = std::chrono::steady_clock::now();
    std::string fingerprint;
    std::lock_guard<std::mutex> lock(g_cachedCatalogMutex);
    if (listData == g_cachedCatalogPayload) {
        entries = g_cachedCatalogEntries;
        fingerprint = g_cachedCatalogFingerprint;
    } else {
        const auto parsed = mvr::gdtf_catalog_parser::ParseCatalog(listData);
        entries = parsed.entries;
        fingerprint = parsed.payloadFingerprint;
        g_cachedCatalogPayload = listData;
        g_cachedCatalogFingerprint = fingerprint;
        g_cachedCatalogEntries = entries;
    }
    lastParseMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - parseStart)
                      .count();
    MaybeLogVerboseCatalogTrace(
        wxString::Format("ParseList source=%s updated_at='%s' bytes=%zu fingerprint=%s parsed_entries=%zu parse_ms=%lld",
                         catalogSource == GdtfCatalogDisplaySource::Online ? "online" : "cache",
                         wxString::FromUTF8(lastUpdatedAt),
                         listData.size(), wxString::FromUTF8(fingerprint), entries.size(),
                         static_cast<long long>(lastParseMs)));
}

void GdtfSearchDialog::UpdateResults()
{
    const auto filterStart = std::chrono::steady_clock::now();
    if (searchDebounceTimer.IsRunning())
        searchDebounceTimer.Stop();

    const std::string previouslySelectedRid = GetSelectedId();
    filteredIndices.clear();
    visible.clear();
    selectedIndex = -1;

    const auto matches = mvr::gdtf_catalog_parser::FilterCatalogEntries(
        entries, manufacturerCtrl->GetValue().ToStdString(),
        fixtureCtrl->GetValue().ToStdString());
    for (std::size_t index : matches)
        filteredIndices.push_back(static_cast<int>(index));

    lastFilterMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - filterStart)
                       .count();
    MaybeLogVerboseCatalogTrace(wxString::Format(
        "Filtering manufacturer='%s' fixture='%s' matches=%zu filter_ms=%lld",
        manufacturerCtrl->GetValue(), fixtureCtrl->GetValue(),
        filteredIndices.size(), static_cast<long long>(lastFilterMs)));

    currentPage = 0;
    RenderCurrentPage(previouslySelectedRid);
}

void GdtfSearchDialog::OnSearch(wxCommandEvent& WXUNUSED(evt))
{
    UpdateResults();
}

void GdtfSearchDialog::OnSearchTextChanged(wxCommandEvent& evt)
{
    evt.Skip();
    searchDebounceTimer.StartOnce(kSearchDebounceMs);
}

void GdtfSearchDialog::OnSearchDebounceTimer(wxTimerEvent& WXUNUSED(evt))
{
    UpdateResults();
}

void GdtfSearchDialog::RenderCurrentPage(const std::string& previouslySelectedRid)
{
    const auto renderStart = std::chrono::steady_clock::now();
    resultTable->Freeze();
    resultTable->DeleteAllItems();
    visible.clear();
    selectedIndex = -1;

    const size_t begin = currentPage * pageSize;
    const size_t end = std::min(begin + pageSize, filteredIndices.size());
    for (size_t pos = begin; pos < end; ++pos) {
        const int entryIndex = filteredIndices[pos];
        const auto& entry = entries[entryIndex];
        visible.push_back(entryIndex);

        wxVector<wxVariant> row;
        row.push_back(wxString::FromUTF8(entry.manufacturer));
        row.push_back(wxString::FromUTF8(entry.fixtureName));
        row.push_back(wxString::FromUTF8(FormatModes(entry.modes)));
        row.push_back(wxString::FromUTF8(entry.creator));
        row.push_back(wxString::FromUTF8(entry.uploader));
        row.push_back(FormatTimestamp(entry.creationDate));
        row.push_back(wxString::FromUTF8(entry.revision));
        row.push_back(FormatTimestamp(entry.lastModifiedUnix > 0
                                          ? std::to_string(entry.lastModifiedUnix)
                                          : std::string{}));
        row.push_back(wxString::FromUTF8(entry.version));
        row.push_back(wxString::FromUTF8(entry.ratingText));
        resultTable->AppendItem(row);

        if (!previouslySelectedRid.empty() && entry.rid == previouslySelectedRid) {
            const unsigned int rowIndex =
                static_cast<unsigned int>(resultTable->GetItemCount() - 1);
            wxDataViewItem rowItem = resultTable->RowToItem(rowIndex);
            if (rowItem.IsOk()) {
                resultTable->Select(rowItem);
                selectedIndex = entryIndex;
            }
        }
    }
    resultTable->Thaw();
    lastRenderMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - renderStart)
                       .count();
    MaybeLogVerboseCatalogTrace(wxString::Format(
        "Visible results page=%zu visible=%zu render_ms=%lld",
        currentPage + 1, visible.size(), static_cast<long long>(lastRenderMs)));
    UpdatePaginationControls();
}

void GdtfSearchDialog::UpdatePaginationControls()
{
    const size_t totalRows = filteredIndices.size();
    const size_t pageCount = totalRows == 0 ? 1 : ((totalRows - 1) / pageSize) + 1;
    if (currentPage >= pageCount)
        currentPage = pageCount - 1;

    prevPageButton->Enable(currentPage > 0);
    nextPageButton->Enable((currentPage + 1) < pageCount);
    pageInfoLabel->SetLabel(wxString::Format(_("Page %zu/%zu (%zu results)"),
                                             currentPage + 1, pageCount, totalRows));
}

void GdtfSearchDialog::OnPrevPage(wxCommandEvent& WXUNUSED(evt))
{
    if (currentPage == 0)
        return;
    --currentPage;
    RenderCurrentPage({});
}

void GdtfSearchDialog::OnNextPage(wxCommandEvent& WXUNUSED(evt))
{
    const size_t totalRows = filteredIndices.size();
    const size_t pageCount = totalRows == 0 ? 1 : ((totalRows - 1) / pageSize) + 1;
    if ((currentPage + 1) >= pageCount)
        return;
    ++currentPage;
    RenderCurrentPage({});
}

void GdtfSearchDialog::OnDownload(wxCommandEvent& WXUNUSED(evt))
{
    if (!FinishRefreshBeforeModalClose())
        return;
    wxDataViewItem item = resultTable->GetSelection();
    int row = resultTable->ItemToRow(item);
    if (row != wxNOT_FOUND && row < static_cast<int>(visible.size())) {
        selectedIndex = visible[row];
        if (!entries[selectedIndex].downloadable) {
            UpdateStatusMessage(false, _("This catalog row has no downloadable revision identifier."));
            return;
        }
        EndModal(wxID_OK);
    }
}

std::string GdtfSearchDialog::GetSelectedId() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
        return entries[selectedIndex].rid;
    return {};
}

std::string GdtfSearchDialog::GetSelectedUrl() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
        return entries[selectedIndex].url;
    return {};
}

std::string GdtfSearchDialog::GetSelectedName() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
        return entries[selectedIndex].fixtureName;
    return {};
}

// Returns the complete selected catalog row for resolver consumers.
std::optional<mvr::gdtf_catalog_matcher::GdtfCatalogEntry>
GdtfSearchDialog::GetSelectedEntry() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
        return entries[selectedIndex];
    return std::nullopt;
}

std::string GdtfSearchDialog::GetCurrentListData() const
{
    return currentListData;
}

void GdtfSearchDialog::OnDialogShown(wxShowEvent& evt)
{
    evt.Skip();
    if (!evt.IsShown())
        return;

    TriggerAutoRefreshOnce();
}

void GdtfSearchDialog::TriggerAutoRefreshOnce()
{
    if (autoRefreshTriggered || !refreshCatalogFn)
        return;
    autoRefreshTriggered = true;
    autoRefreshInProgress = true;
    if (downloadButton)
        downloadButton->Disable();
    UpdateStatusMessage(true);

    autoRefreshThread = std::thread([this, refreshFn = refreshCatalogFn]() {
        const auto refreshStart = std::chrono::steady_clock::now();
        RefreshResult result = refreshFn();
        result.refreshMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - refreshStart)
                               .count();
        if (result.success && !result.listData.empty())
        {
            const auto parseStart = std::chrono::steady_clock::now();
            const auto parsed = mvr::gdtf_catalog_parser::ParseCatalog(result.listData);
            result.parsedEntries = parsed.entries;
            result.payloadFingerprint = parsed.payloadFingerprint;
            result.parseMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - parseStart)
                                 .count();
        }
        wxThreadEvent* event = new wxThreadEvent(EVT_GDTF_REFRESH_DONE);
        event->SetPayload(result);
        wxQueueEvent(this, event);
    });
}

void GdtfSearchDialog::OnAutoRefreshThreadEvent(wxThreadEvent& evt)
{
    if (autoRefreshThread.joinable())
        autoRefreshThread.join();
    autoRefreshInProgress = false;
    OnAutoRefreshFinished(evt.GetPayload<RefreshResult>());
    if (downloadButton)
        downloadButton->Enable(std::any_of(entries.begin(), entries.end(),
            [](const auto& entry) { return entry.downloadable; }));
}

// Applies refreshed catalog data and updates status text after the background refresh completes.
void GdtfSearchDialog::OnAutoRefreshFinished(const RefreshResult& result)
{
    wxLogTrace("gdtf", "GDTF metrics: refresh_ms=%lld parse_ms=%lld ui_parse_ms=%lld filter_ms=%lld render_ms=%lld",
                 static_cast<long long>(result.refreshMs),
                 static_cast<long long>(result.parseMs),
                 static_cast<long long>(lastParseMs),
                 static_cast<long long>(lastFilterMs),
                 static_cast<long long>(lastRenderMs));
    if (result.success && !result.listData.empty() && !result.parsedEntries.empty()) {
        currentListData = result.listData;
        lastUpdatedAt = result.updatedAt;
        catalogSource = result.source == GdtfCatalogDisplaySource::None
                            ? GdtfCatalogDisplaySource::Online
                            : result.source;
        downloadRequiresAuthentication = false;

        {
            std::lock_guard<std::mutex> lock(g_cachedCatalogMutex);
            g_cachedCatalogPayload = currentListData;
            g_cachedCatalogFingerprint = result.payloadFingerprint;
            g_cachedCatalogEntries = result.parsedEntries;
        }
        entries = result.parsedEntries;
        UpdateResults();
        UpdateStatusMessage(false);
        return;
    }

    catalogSource = entries.empty() ? GdtfCatalogDisplaySource::None : GdtfCatalogDisplaySource::Cached;
    wxString fallbackDetails = lastUpdatedAt.empty()
        ? _("Showing cached GDTF catalog. Sign in is required to download.")
        : wxString::Format(
              _("Showing cached GDTF catalog (last updated: %s). Sign in is required to download."),
              wxString::FromUTF8(lastUpdatedAt));
    if (!result.failureDetails.empty())
        fallbackDetails += wxString::Format(" - %s", wxString::FromUTF8(result.failureDetails));
    UpdateStatusMessage(false, fallbackDetails);

    if (entries.empty() && !result.failureDetails.empty()) {
        wxMessageBox(wxString::Format(_("Online GDTF catalog refresh failed.\n%s"),
                         wxString::FromUTF8(result.failureDetails)),
                     _("GDTF catalog refresh"), wxOK | wxICON_WARNING, this);
    }
}

bool GdtfSearchDialog::FinishRefreshBeforeModalClose()
{
    if (!autoRefreshInProgress)
        return true;
    UpdateStatusMessage(false, _("Please wait for the online catalog refresh to finish before downloading."));
    return false;
}

void GdtfSearchDialog::MaybeLogVerboseCatalogTrace(const wxString& message) const
{
    if (!ui::IsFeatureEnabled(ui::FeatureFlag::GdtfVerboseCatalogLogs))
        return;
    wxLogDebug("[GDTF] %s", message);
}

void GdtfSearchDialog::UpdateStatusMessage(bool refreshing, const wxString& details)
{
    if (refreshing) {
        statusLabel->SetLabel(_("Updating online catalog..."));
        return;
    }

    if (!details.empty()) {
        statusLabel->SetLabel(details);
        return;
    }

    if (catalogSource == GdtfCatalogDisplaySource::Online) {
        statusLabel->SetLabel(_("Showing online GDTF catalog."));
        return;
    }

    if (catalogSource == GdtfCatalogDisplaySource::None || entries.empty()) {
        statusLabel->SetLabel(_("No GDTF catalog is available."));
        return;
    }

    if (downloadRequiresAuthentication) {
        if (lastUpdatedAt.empty())
            statusLabel->SetLabel(_("Showing cached GDTF catalog. Sign in is required to download."));
        else
            statusLabel->SetLabel(wxString::Format(
                _("Showing cached GDTF catalog (last updated: %s). Sign in is required to download."),
                wxString::FromUTF8(lastUpdatedAt)));
        return;
    }

    if (lastUpdatedAt.empty())
        statusLabel->SetLabel(_("Showing cached GDTF catalog."));
    else
        statusLabel->SetLabel(wxString::Format(
            _("Showing cached GDTF catalog (last updated: %s)."),
            wxString::FromUTF8(lastUpdatedAt)));
}
