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
#include <vector>
#include "../external/json.hpp"

struct GdtfEntry {
    std::string manufacturer;
    std::string fixture;
    std::string rid;
    std::string url;
    std::string modes;
    std::string creator;
    std::string uploader;
    std::string creationDate;
    std::string revision;
    std::string lastModified;
    std::string version;
    std::string rating;
};

class GdtfSearchDialog : public wxDialog {
public:
    GdtfSearchDialog(wxWindow* parent, const std::string& listData);
    std::string GetSelectedId() const;
    std::string GetSelectedUrl() const;
    std::string GetSelectedName() const;
private:
    void ParseList(const std::string& listData);
    void UpdateResults();
    void OnSearch(wxCommandEvent& evt);
    void OnDownload(wxCommandEvent& evt);

    wxTextCtrl* manufacturerCtrl = nullptr;
    wxTextCtrl* fixtureCtrl = nullptr;
    wxDataViewListCtrl* resultTable = nullptr;
    std::vector<GdtfEntry> entries;
    std::vector<int> visible;
    int selectedIndex = -1;
};
