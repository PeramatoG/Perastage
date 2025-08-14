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
#include <wx/checklst.h>
#include <vector>
#include <string>

class ColumnSelectionDialog : public wxDialog {
public:
    ColumnSelectionDialog(wxWindow* parent,
                          const std::vector<std::string>& columns,
                          const std::vector<int>& selected = {});
    std::vector<int> GetSelectedColumns() const;

private:
    void OnUp(wxCommandEvent& evt);
    void OnDown(wxCommandEvent& evt);
    void OnSelectAll(wxCommandEvent& evt);
    void OnDeselectAll(wxCommandEvent& evt);

    wxCheckListBox* list = nullptr;
    std::vector<int> indices; // maps list items to original column indices
};
