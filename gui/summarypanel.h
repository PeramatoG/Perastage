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

// Panel that shows a summary count of items by type/model/name
class SummaryPanel : public wxPanel {
public:
    explicit SummaryPanel(wxWindow* parent);

  void ShowFixtureSummary();
  void ShowTrussSummary();
  void ShowSceneObjectSummary();
  void ShowSupportSummary();

    static SummaryPanel* Instance();
  static void SetInstance(SummaryPanel* panel);

private:
  wxDataViewListCtrl* table = nullptr;
  void ResetColumns(const std::vector<wxString>& headers);
  void ShowSummary(const std::vector<std::pair<std::string,int>>& items);
};
