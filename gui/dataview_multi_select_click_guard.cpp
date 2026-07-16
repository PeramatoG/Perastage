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
  BindMouseEvents();
}

// Cancels pending work before the guarded table owner is destroyed.
DataViewMultiSelectClickGuard::~DataViewMultiSelectClickGuard() {
  CancelPendingClick();
  Unbind(wxEVT_TIMER, &DataViewMultiSelectClickGuard::OnTimer, this);
  UnbindMouseEvents();
}

// Handles bound left-down events before the native table changes selection.
void DataViewMultiSelectClickGuard::OnBoundLeftDown(wxMouseEvent &event) {
  if (HandleLeftDown(event))
    return;
  event.Skip();
}

// Handles bound double-click events before the native table changes selection.
void DataViewMultiSelectClickGuard::OnBoundLeftDClick(wxMouseEvent &event) {
  if (HandleLeftDClick(event))
    return;
  event.Skip();
}

// Suppresses left-up while a click is pending so native selection waits too.
void DataViewMultiSelectClickGuard::OnBoundLeftUp(wxMouseEvent &event) {
  if (HasPendingClick())
    return;
  event.Skip();
}

// Defers plain clicks on already-selected rows when a multi-selection is active.
bool DataViewMultiSelectClickGuard::HandleLeftDown(wxMouseEvent &event) {
  if (!table)
    return false;

  wxDataViewItem item;
  wxDataViewColumn *column = nullptr;
  table->HitTest(TablePosition(event), item, column);
  const int row = item.IsOk() ? table->ItemToRow(item) : wxNOT_FOUND;

  if (HasPendingClick()) {
    if (row == pendingRow && !event.ControlDown() && !event.ShiftDown())
      return true;
    CancelPendingClick();
  }

  if (!ShouldDelayClick(event, item, row))
    return false;

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
  table->HitTest(TablePosition(event), item, column);
  const int row = item.IsOk() ? table->ItemToRow(item) : wxNOT_FOUND;
  if (row != pendingRow) {
    CancelPendingClick();
    return false;
  }

  wxDataViewColumn *editColumn = column ? column : pendingColumn;
  const int editColumnIndex = editColumn ? table->GetColumnPosition(editColumn)
                                        : wxNOT_FOUND;
  wxDataViewItem editItem = item.IsOk() ? item : pendingItem;
  CancelPendingClick();
  if (doubleClickHandler)
    doubleClickHandler(editItem, editColumnIndex);
  return true;
}

// Cancels any delayed single-click action.
void DataViewMultiSelectClickGuard::CancelPendingClick() {
  if (pendingTimer.IsRunning())
    pendingTimer.Stop();
  pendingItem = wxDataViewItem();
  pendingColumn = nullptr;
  pendingRow = wxNOT_FOUND;
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

// Returns the platform double-click interval exposed by wxWidgets.
int DataViewMultiSelectClickGuard::DoubleClickIntervalMs() const {
  const int interval = wxSystemSettings::GetMetric(wxSYS_DCLICK_MSEC);
  return interval > 0 ? interval : 250;
}

// Converts mouse coordinates from child windows into table client coordinates.
wxPoint DataViewMultiSelectClickGuard::TablePosition(
    const wxMouseEvent &event) const {
  wxWindow *sourceWindow = dynamic_cast<wxWindow *>(event.GetEventObject());
  if (!table || !sourceWindow || sourceWindow == table)
    return event.GetPosition();
  return table->ScreenToClient(sourceWindow->ClientToScreen(event.GetPosition()));
}

// Binds mouse interception to the table and its native child windows.
void DataViewMultiSelectClickGuard::BindMouseEvents() {
  BindMouseEvents(table);
  if (!table)
    return;
  for (wxWindow *child : table->GetChildren())
    BindMouseEvents(child);
}

// Unbinds mouse interception from the table and its native child windows.
void DataViewMultiSelectClickGuard::UnbindMouseEvents() {
  UnbindMouseEvents(table);
  if (!table)
    return;
  for (wxWindow *child : table->GetChildren())
    UnbindMouseEvents(child);
}

// Binds mouse interception to one table window.
void DataViewMultiSelectClickGuard::BindMouseEvents(wxWindow *window) {
  if (!window)
    return;
  window->Bind(wxEVT_LEFT_DOWN, &DataViewMultiSelectClickGuard::OnBoundLeftDown,
               this);
  window->Bind(wxEVT_LEFT_DCLICK,
               &DataViewMultiSelectClickGuard::OnBoundLeftDClick, this);
  window->Bind(wxEVT_LEFT_UP, &DataViewMultiSelectClickGuard::OnBoundLeftUp,
               this);
}

// Unbinds mouse interception from one table window.
void DataViewMultiSelectClickGuard::UnbindMouseEvents(wxWindow *window) {
  if (!window)
    return;
  window->Unbind(wxEVT_LEFT_DOWN,
                 &DataViewMultiSelectClickGuard::OnBoundLeftDown, this);
  window->Unbind(wxEVT_LEFT_DCLICK,
                 &DataViewMultiSelectClickGuard::OnBoundLeftDClick, this);
  window->Unbind(wxEVT_LEFT_UP, &DataViewMultiSelectClickGuard::OnBoundLeftUp,
                 this);
}
