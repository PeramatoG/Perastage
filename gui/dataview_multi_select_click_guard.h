/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
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

#include <functional>
#include <vector>
#include <wx/dataview.h>
#include <wx/timer.h>

class DataViewMultiSelectClickGuard : public wxEvtHandler {
public:
  using DoubleClickHandler =
      std::function<void(const wxDataViewItem &, int, const std::vector<int> &)>;

  DataViewMultiSelectClickGuard(wxDataViewListCtrl *table,
                                DoubleClickHandler doubleClickHandler);
  ~DataViewMultiSelectClickGuard() override;

  bool HandleLeftDown(wxMouseEvent &event);
  bool HandleLeftDClick(wxMouseEvent &event);
  void CancelPendingClick();
  bool HasPendingClick() const;

private:
  void OnTimer(wxTimerEvent &event);
  void OnSelectionChanged(wxDataViewEvent &event);
  bool ShouldDelayClick(const wxMouseEvent &event, const wxDataViewItem &item,
                        int row) const;
  void RestorePendingSelectionSilently() const;
  std::vector<int> CurrentSelectionRows() const;
  bool ContainsRow(const std::vector<int> &rows, int row) const;
  int DoubleClickIntervalMs() const;

  wxDataViewListCtrl *table = nullptr;
  DoubleClickHandler doubleClickHandler;
  wxTimer pendingTimer;
  wxDataViewItem pendingItem;
  wxDataViewColumn *pendingColumn = nullptr;
  int pendingRow = wxNOT_FOUND;
  std::vector<int> pendingSelectionRows;
  std::vector<int> lastMultiSelectionRows;
};
