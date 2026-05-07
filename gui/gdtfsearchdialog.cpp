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
#include <array>
#include <chrono>
#include <mutex>
#include <wx/datetime.h>
#include <wx/log.h>

using json = nlohmann::json;
wxDEFINE_EVENT(EVT_GDTF_REFRESH_DONE, wxThreadEvent);

namespace {
wxString FormatTimestamp(const std::string& ts);

std::string NormalizeSearchToken(const std::string& text)
{
    wxString normalized = wxString::FromUTF8(text).Lower();
    normalized.Replace(" ", "");
    normalized.Replace("-", "");
    return normalized.ToStdString();
}

// Finds the first JSON array node that looks like a fixture catalog payload.
const json* FindFixtureArrayNode(const json& node)
{
    if (node.is_array())
        return &node;
    if (!node.is_object())
        return nullptr;

    static const std::array<const char*, 8> kArrayKeys = {
        "data", "fixtures", "list", "results", "items", "docs", "rows", "catalog"
    };
    for (const char* key : kArrayKeys) {
        auto it = node.find(key);
        if (it == node.end())
            continue;
        if (it->is_array())
            return &(*it);
        if (const json* nested = FindFixtureArrayNode(*it))
            return nested;
    }
    return nullptr;
}

// Parses raw catalog JSON text into normalized GDTF search entries.
std::vector<GdtfEntry> ParseEntriesFromListData(const std::string& listData)
{
    std::vector<GdtfEntry> parsedEntries;
    json j = json::parse(listData, nullptr, false);
    if (j.is_discarded())
        return parsedEntries;

    const json* payloadArray = FindFixtureArrayNode(j);
    if (!payloadArray)
        return parsedEntries;

    auto jsonToString = [](const json& v) -> std::string {
        if (v.is_string())
            return v.get<std::string>();
        if (v.is_number())
            return v.dump();
        if (v.is_array()) {
            std::string result;
            for (size_t i = 0; i < v.size(); ++i) {
                if (i > 0)
                    result += ", ";
                const auto& el = v[i];
                if (el.is_string())
                    result += el.get<std::string>();
                else if (el.is_object() && el.contains("name") && el["name"].is_string())
                    result += el["name"].get<std::string>();
                else
                    result += el.dump();
            }
            return result;
        }
        if (v.is_object())
            return v.dump();
        return {};
    };

    auto getValue = [&](const json& obj, std::initializer_list<const char*> keys) -> std::string {
        for (const char* k : keys) {
            auto it = obj.find(k);
            if (it != obj.end())
                return jsonToString(*it);
        }
        return {};
    };

    parsedEntries.reserve(payloadArray->size());
    for (const auto& item : *payloadArray) {
        GdtfEntry e;
        e.manufacturer = getValue(item, {"manufacturer", "brand", "mfr"});
        e.fixture = getValue(item, {"fixture", "name", "model"});
        e.manufacturerNorm = NormalizeSearchToken(e.manufacturer);
        e.fixtureNorm = NormalizeSearchToken(e.fixture);
        e.rid = getValue(item, {"rid", "revisionId"});
        e.url = getValue(item, {"url", "download", "downloadUrl"});
        e.modes = getValue(item, {"modes", "mode", "modeCount"});
        e.creator = getValue(item, {"creator", "user", "userName"});
        e.uploader = getValue(item, {"uploader"});
        e.creationDate = getValue(item, {"creationDate"});
        e.creationDateDisplay = FormatTimestamp(e.creationDate).ToStdString();
        e.revision = getValue(item, {"revision"});
        e.lastModified = getValue(item, {"lastModified"});
        e.lastModifiedDisplay = FormatTimestamp(e.lastModified).ToStdString();
        e.version = getValue(item, {"version"});
        e.rating = getValue(item, {"rating"});
        parsedEntries.push_back(std::move(e));
    }
    return parsedEntries;
}

std::mutex g_cachedCatalogMutex;
std::string g_cachedCatalogPayload;
std::vector<GdtfEntry> g_cachedCatalogEntries;

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
                                   RefreshCatalogFn refreshCatalogFnIn)
    : wxDialog(parent, wxID_ANY, "Search GDTF", wxDefaultPosition,
               wxSize(1000,700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      currentListData(listData),
      lastUpdatedAt(cachedUpdatedAt),
      refreshCatalogFn(std::move(refreshCatalogFnIn)),
      searchDebounceTimer(this)
{
    pageSize = kDefaultPageSize;
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
    searchSizer->Add(new wxStaticText(this, wxID_ANY, "Manufacturer:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);
    manufacturerCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                     wxDefaultSize, wxTE_PROCESS_ENTER);
    searchSizer->Add(manufacturerCtrl, 1, wxRIGHT, 10);
    searchSizer->Add(new wxStaticText(this, wxID_ANY, "Fixture:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);
    fixtureCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                 wxDefaultSize, wxTE_PROCESS_ENTER);
    searchSizer->Add(fixtureCtrl, 1);
    sizer->Add(searchSizer, 0, wxEXPAND | wxALL, 10);

    statusLabel = new wxStaticText(this, wxID_ANY, "");
    sizer->Add(statusLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* pageSizer = new wxBoxSizer(wxHORIZONTAL);
    prevPageButton = new wxButton(this, wxID_ANY, "< Prev");
    nextPageButton = new wxButton(this, wxID_ANY, "Next >");
    pageInfoLabel = new wxStaticText(this, wxID_ANY, "Page 1/1");
    pageSizer->Add(prevPageButton, 0, wxRIGHT, 8);
    pageSizer->Add(nextPageButton, 0, wxRIGHT, 10);
    pageSizer->Add(pageInfoLabel, 0, wxALIGN_CENTER_VERTICAL);
    pageSizer->AddStretchSpacer(1);
    sizer->Add(pageSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    resultTable = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                         wxDefaultSize, wxDV_ROW_LINES);
    int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
    resultTable->AppendTextColumn("Manufacturer", wxDATAVIEW_CELL_INERT, 150,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Fixture", wxDATAVIEW_CELL_INERT, 200,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Modes", wxDATAVIEW_CELL_INERT, 60,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Creator", wxDATAVIEW_CELL_INERT, 120,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Uploader", wxDATAVIEW_CELL_INERT, 100,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Creation Date", wxDATAVIEW_CELL_INERT, 110,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Revision", wxDATAVIEW_CELL_INERT, 90,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Last Modified", wxDATAVIEW_CELL_INERT, 110,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Version", wxDATAVIEW_CELL_INERT, 80,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Rating", wxDATAVIEW_CELL_INERT, 60,
                                  wxALIGN_LEFT, flags);
    ColumnUtils::EnforceMinColumnWidth(resultTable);
    sizer->Add(resultTable, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* downloadBtn = new wxButton(this, wxID_OK, "Download");
    wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");
    btnSizer->AddStretchSpacer(1);
    btnSizer->Add(downloadBtn, 0, wxRIGHT, 5);
    btnSizer->Add(cancelBtn, 0);
    sizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(sizer);
    SetMinSize(wxSize(800, 600));
    SetSize(wxSize(1000, 700));

    manufacturerCtrl->Bind(wxEVT_TEXT_ENTER, &GdtfSearchDialog::OnSearch, this);
    fixtureCtrl->Bind(wxEVT_TEXT_ENTER, &GdtfSearchDialog::OnSearch, this);
    manufacturerCtrl->Bind(wxEVT_TEXT, &GdtfSearchDialog::OnSearchTextChanged, this);
    fixtureCtrl->Bind(wxEVT_TEXT, &GdtfSearchDialog::OnSearchTextChanged, this);
    downloadBtn->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnDownload, this);
    prevPageButton->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnPrevPage, this);
    nextPageButton->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnNextPage, this);
    resultTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                      &GdtfSearchDialog::OnDownload, this);
    Bind(wxEVT_SHOW, &GdtfSearchDialog::OnDialogShown, this);
    Bind(EVT_GDTF_REFRESH_DONE, &GdtfSearchDialog::OnAutoRefreshThreadEvent, this);
    Bind(wxEVT_TIMER, &GdtfSearchDialog::OnSearchDebounceTimer, this,
         searchDebounceTimer.GetId());

    ParseList(currentListData);
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
    std::lock_guard<std::mutex> lock(g_cachedCatalogMutex);
    if (listData == g_cachedCatalogPayload) {
        entries = g_cachedCatalogEntries;
    } else {
        entries = ParseEntriesFromListData(listData);
        g_cachedCatalogPayload = listData;
        g_cachedCatalogEntries = entries;
    }
    lastParseMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - parseStart)
                      .count();
    MaybeLogVerboseCatalogTrace(
        wxString::Format("ParseList size=%zu parsed_entries=%zu parse_ms=%lld",
                         listData.size(), entries.size(),
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

    const std::string manufacturerSearch =
        NormalizeSearchToken(manufacturerCtrl->GetValue().ToStdString());
    const std::string fixtureSearch =
        NormalizeSearchToken(fixtureCtrl->GetValue().ToStdString());
    for (size_t i = 0; i < entries.size(); ++i) {
        const GdtfEntry& entry = entries[i];
        if ((!manufacturerSearch.empty() &&
             entry.manufacturerNorm.find(manufacturerSearch) == std::string::npos) ||
            (!fixtureSearch.empty() &&
             entry.fixtureNorm.find(fixtureSearch) == std::string::npos))
            continue;
        filteredIndices.push_back(static_cast<int>(i));
    }

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
        const GdtfEntry& entry = entries[entryIndex];
        visible.push_back(entryIndex);

        wxVector<wxVariant> row;
        row.push_back(wxString::FromUTF8(entry.manufacturer));
        row.push_back(wxString::FromUTF8(entry.fixture));
        row.push_back(wxString::FromUTF8(entry.modes));
        row.push_back(wxString::FromUTF8(entry.creator));
        row.push_back(wxString::FromUTF8(entry.uploader));
        row.push_back(wxString::FromUTF8(entry.creationDateDisplay));
        row.push_back(wxString::FromUTF8(entry.revision));
        row.push_back(wxString::FromUTF8(entry.lastModifiedDisplay));
        row.push_back(wxString::FromUTF8(entry.version));
        row.push_back(wxString::FromUTF8(entry.rating));
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
    pageInfoLabel->SetLabel(wxString::Format("Page %zu/%zu (%zu results)",
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
    wxDataViewItem item = resultTable->GetSelection();
    int row = resultTable->ItemToRow(item);
    if (row != wxNOT_FOUND && row < static_cast<int>(visible.size())) {
        selectedIndex = visible[row];
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
        return entries[selectedIndex].fixture;
    return {};
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
            result.parsedEntries = ParseEntriesFromListData(result.listData);
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
    OnAutoRefreshFinished(evt.GetPayload<RefreshResult>());
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

        {
            std::lock_guard<std::mutex> lock(g_cachedCatalogMutex);
            g_cachedCatalogPayload = currentListData;
            g_cachedCatalogEntries = result.parsedEntries;
        }
        entries = result.parsedEntries;
        UpdateResults();
        UpdateStatusMessage(false);
        return;
    }

    UpdateStatusMessage(false,
                        "Showing local catalog (last updated: " +
                            wxString::FromUTF8(lastUpdatedAt) + ")");
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
        statusLabel->SetLabel("Updating online catalog...");
        return;
    }

    if (!details.empty()) {
        statusLabel->SetLabel(details);
        return;
    }

    if (lastUpdatedAt.empty())
        statusLabel->SetLabel("Showing local catalog.");
    else
        statusLabel->SetLabel("Showing local catalog (last updated: " +
                              wxString::FromUTF8(lastUpdatedAt) + ")");
}
