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
#include <string>
#include <vector>
#include "colorstore.h"

class IGuiConfigServices;

class HoistTablePanel : public wxPanel {
public:
  explicit HoistTablePanel(wxWindow *parent, IGuiConfigServices *services = nullptr);
  ~HoistTablePanel();

  void ReloadData();
  void HighlightHoist(const std::string &uuid);
  void ClearSelection();
  std::vector<std::string> GetSelectedUuids() const;
  void SelectByUuid(const std::vector<std::string> &uuids);
  bool IsActivePage() const;
  void DeleteSelected();
  wxDataViewListCtrl *GetTableCtrl() const { return table; }

  static HoistTablePanel *Instance();
  static void SetInstance(HoistTablePanel *panel);

  void UpdateSceneData();

private:
  ColorfulDataViewListStore *store;
  wxDataViewListCtrl *table;
  std::vector<wxString> columnLabels;
  std::vector<std::string> rowUuids;
  bool dragSelecting = false;
  int startRow = -1;
  IGuiConfigServices *guiConfigServices = nullptr;

  void InitializeTable();
  void OnSelectionChanged(wxDataViewEvent &evt);
  void OnContextMenu(wxDataViewEvent &event);
  void OnColumnSorted(wxDataViewEvent &event);
  void ResyncRows(const std::vector<std::string> &oldOrder,
                  const std::vector<std::string> &selectedUuids);
  void OnLeftDown(wxMouseEvent &evt);
  void OnLeftUp(wxMouseEvent &evt);
  void OnMouseMove(wxMouseEvent &evt);
  void OnCaptureLost(wxMouseCaptureLostEvent &evt);
  void UpdateSelectionHighlight();
};
