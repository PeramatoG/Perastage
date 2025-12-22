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

#include <wx/dataview.h>
#include <wx/wx.h>

wxDECLARE_EVENT(EVT_LAYOUT_SELECTED, wxCommandEvent);

class LayoutPanel : public wxPanel {
public:
  explicit LayoutPanel(wxWindow *parent);
  void ReloadLayouts();

  static LayoutPanel *Instance();
  static void SetInstance(LayoutPanel *p);

private:
  void OnSelect(wxDataViewEvent &evt);
  void OnAddLayout(wxCommandEvent &evt);
  void OnRenameLayout(wxCommandEvent &evt);
  void OnDeleteLayout(wxCommandEvent &evt);
  void EmitLayoutSelected(const std::string &viewId);

  wxDataViewListCtrl *list = nullptr;
  std::string currentLayout;

  static LayoutPanel *s_instance;
};
