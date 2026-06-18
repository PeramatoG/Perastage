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
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "colorstore.h"
#include "hoist_load_limit_utils.h"
#include "positionvalueupdate.h"

class IGuiConfigServices;

class HoistTablePanel : public wxPanel {
public:
  explicit HoistTablePanel(wxWindow *parent, IGuiConfigServices *services = nullptr);
  ~HoistTablePanel();

  void ReloadData();
  void HighlightHoist(const std::string &uuid);
  void HighlightHoist(const std::string &uuid,
                      const std::vector<std::string> &relatedUuids);
  void ClearSelection();
  std::vector<std::string> GetSelectedUuids() const;
  void SelectByUuid(const std::vector<std::string> &uuids,
                    bool notifySelectionChanged = true);
  bool IsActivePage() const;
  void DeleteSelected(bool pushUndoState = true);
  void ApplyPositionValueUpdates(const std::vector<PositionValueUpdate> &updates);
  wxDataViewListCtrl *GetTableCtrl() const { return table; }

  static HoistTablePanel *Instance();
  static void SetInstance(HoistTablePanel *panel);

  void UpdateSceneData(bool logChanges = true);

private:
  ColorfulDataViewListStore *store;
  wxDataViewListCtrl *table;
  std::vector<wxString> columnLabels;
  std::vector<std::string> rowUuids;
  std::vector<HoistLoadLimitUtils::LoadLimitState> rowLoadStates;
  std::unordered_map<wxUIntPtr, std::string> rowUuidByKey;
  std::unordered_map<wxUIntPtr, HoistLoadLimitUtils::LoadLimitState> loadStateByKey;
  std::unordered_set<std::string> pendingManualLoadUuids;
  wxUIntPtr nextRowKey = 1;
  wxString activeHoverTooltip;
  bool dragSelecting = false;
  int startRow = -1;
  IGuiConfigServices *guiConfigServices = nullptr;

  void InitializeTable();
  void OnSelectionChanged(wxDataViewEvent &evt);
  void OnContextMenu(wxDataViewEvent &event);
  void OnColumnSorted(wxDataViewEvent &event);
  void RebuildRowCachesFromRowKeys();
  std::string UuidForItem(const wxDataViewItem &item) const;
  void SetLoadStateForRow(unsigned int row,
                          const HoistLoadLimitUtils::LoadLimitState &state);
  void ResyncRows(const std::vector<std::string> &oldOrder,
                  const std::vector<std::string> &selectedUuids);
  void OnLeftDown(wxMouseEvent &evt);
  void OnLeftUp(wxMouseEvent &evt);
  void OnMouseMove(wxMouseEvent &evt);
  void OnMouseLeave(wxMouseEvent &evt);
  void OnCaptureLost(wxMouseCaptureLostEvent &evt);
  void UpdateHoverTooltip(const wxPoint &position);
  void UpdateSelectionHighlight();
};
