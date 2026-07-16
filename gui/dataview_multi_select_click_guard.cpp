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
#include "dataview_multi_select_click_guard.h"

#include <algorithm>
#include <utility>
#include <wx/settings.h>

// Creates a guard that defers selected-row clicks long enough to detect double-clicks.
DataViewMultiSelectClickGuard::DataViewMultiSelectClickGuard(
    wxDataViewListCtrl *table, DoubleClickHandler doubleClickHandler)
    : table(table), doubleClickHandler(std::move(doubleClickHandler)),
      pendingTimer(this) {
  Bind(wxEVT_TIMER, &DataViewMultiSelectClickGuard::OnTimer, this);
}

// Cancels pending work before the guarded table owner is destroyed.
DataViewMultiSelectClickGuard::~DataViewMultiSelectClickGuard() {
  CancelPendingClick();
  Unbind(wxEVT_TIMER, &DataViewMultiSelectClickGuard::OnTimer, this);
}

// Defers plain clicks on already-selected rows when a multi-selection is active.
bool DataViewMultiSelectClickGuard::HandleLeftDown(wxMouseEvent &event) {
  CancelPendingClick();
  if (!table)
    return false;

  wxDataViewItem item;
  wxDataViewColumn *column = nullptr;
  table->HitTest(event.GetPosition(), item, column);
  const int row = item.IsOk() ? table->ItemToRow(item) : wxNOT_FOUND;
  if (!ShouldDelayClick(event, item, row))
    return false;

  wxDataViewItemArray selections;
  table->GetSelections(selections);
  pendingSelectionRows.clear();
  pendingSelectionRows.reserve(selections.size());
  for (const auto &selection : selections) {
    const int selectedRow = table->ItemToRow(selection);
    if (selectedRow != wxNOT_FOUND)
      pendingSelectionRows.push_back(selectedRow);
  }

  pendingItem = item;
  pendingColumn = column;
  pendingRow = row;
  pendingTimer.StartOnce(DoubleClickIntervalMs());
  return true;
}

// Resolves a pending click as a batch edit when the second click arrives in time.
bool DataViewMultiSelectClickGuard::HandleLeftDClick(wxMouseEvent &event) {
  if (!HasPendingClick() || !table)
    return false;

  wxDataViewItem item;
  wxDataViewColumn *column = nullptr;
  table->HitTest(event.GetPosition(), item, column);
  const int row = item.IsOk() ? table->ItemToRow(item) : wxNOT_FOUND;
  if (row != pendingRow) {
    CancelPendingClick();
    return false;
  }

  wxDataViewColumn *editColumn = column ? column : pendingColumn;
  const int editColumnIndex = editColumn ? table->GetColumnPosition(editColumn)
                                        : wxNOT_FOUND;
  wxDataViewItem editItem = item.IsOk() ? item : pendingItem;
  std::vector<int> editSelectionRows = pendingSelectionRows;
  RestorePendingSelectionSilently();
  CancelPendingClick();
  if (doubleClickHandler)
    doubleClickHandler(editItem, editColumnIndex, editSelectionRows);
  return true;
}

// Cancels any delayed single-click action.
void DataViewMultiSelectClickGuard::CancelPendingClick() {
  if (pendingTimer.IsRunning())
    pendingTimer.Stop();
  pendingItem = wxDataViewItem();
  pendingColumn = nullptr;
  pendingRow = wxNOT_FOUND;
  pendingSelectionRows.clear();
}

// Returns true when a delayed single-click action is waiting for resolution.
bool DataViewMultiSelectClickGuard::HasPendingClick() const {
  return pendingRow != wxNOT_FOUND && pendingItem.IsOk();
}

// Converts an unresolved pending click into normal single-row selection.
void DataViewMultiSelectClickGuard::OnTimer(wxTimerEvent &WXUNUSED(event)) {
  if (!table || !HasPendingClick())
    return;

  const int row = pendingRow;
  CancelPendingClick();
  table->UnselectAll();
  if (row != wxNOT_FOUND && row < static_cast<int>(table->GetItemCount()))
    table->SelectRow(static_cast<unsigned int>(row));
}

// Returns true when this click should wait for a possible double-click.
bool DataViewMultiSelectClickGuard::ShouldDelayClick(
    const wxMouseEvent &event, const wxDataViewItem &item, int row) const {
  if (!table || !item.IsOk() || row == wxNOT_FOUND || event.ControlDown() ||
      event.ShiftDown())
    return false;

  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (selections.size() <= 1)
    return false;

  return std::any_of(selections.begin(), selections.end(), [&](const auto &sel) {
    return table->ItemToRow(sel) == row;
  });
}

// Restores the captured multi-selection without notifying selection observers.
void DataViewMultiSelectClickGuard::RestorePendingSelectionSilently() const {
  if (!table || pendingSelectionRows.empty())
    return;

  wxDataViewItemArray currentSelections;
  table->GetSelections(currentSelections);
  if (currentSelections.size() == pendingSelectionRows.size())
    return;

  wxEventBlocker blocker(table, wxEVT_DATAVIEW_SELECTION_CHANGED);
  table->UnselectAll();
  for (const int row : pendingSelectionRows) {
    if (row >= 0 && row < static_cast<int>(table->GetItemCount()))
      table->SelectRow(static_cast<unsigned int>(row));
  }
}

// Returns the platform double-click interval exposed by wxWidgets.
int DataViewMultiSelectClickGuard::DoubleClickIntervalMs() const {
  const int interval = wxSystemSettings::GetMetric(wxSYS_DCLICK_MSEC);
  return interval > 0 ? interval : 250;
}
