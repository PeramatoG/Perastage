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
#include "dataview_deferred_selection_guard.h"

#include <algorithm>
#include <utility>
#include <wx/event.h>
#include <wx/settings.h>
#include <wx/wupdlock.h>

namespace gui {

// Creates a guard and binds mouse hooks to the table item area.
DataViewDeferredSelectionGuard::DataViewDeferredSelectionGuard(
    wxWindow *owner, wxDataViewCtrl *table, ItemKeyProvider itemKeyProvider,
    ItemFinder itemFinder, SelectionSyncCallback selectionSyncCallback,
    HighlightCallback highlightCallback)
    : owner(owner), table(table), itemKeyProvider(std::move(itemKeyProvider)),
      itemFinder(std::move(itemFinder)),
      selectionSyncCallback(std::move(selectionSyncCallback)),
      highlightCallback(std::move(highlightCallback)), timer(owner) {
  if (owner)
    owner->Bind(wxEVT_TIMER, &DataViewDeferredSelectionGuard::OnTimer, this,
                timer.GetId());
  BindMouseEvents();
}

// Unbinds callbacks and stops pending deferred selection work.
DataViewDeferredSelectionGuard::~DataViewDeferredSelectionGuard() {
  Cancel();
  UnbindMouseEvents();
  if (owner)
    owner->Unbind(wxEVT_TIMER, &DataViewDeferredSelectionGuard::OnTimer, this,
                  timer.GetId());
}

// Clears any pending deferred click without synchronizing a selection.
void DataViewDeferredSelectionGuard::Cancel() {
  if (timer.IsRunning())
    timer.Stop();
  pending = false;
  clickedKey.clear();
  clickedModelColumn = wxNOT_FOUND;
  snapshotKeys.clear();
}

// Restores a transient native collapse and reports whether propagation was handled.
bool DataViewDeferredSelectionGuard::HandleSelectionChanged() {
  if (!pending || restoringSelection)
    return false;

  const std::vector<ItemKey> keys = CurrentSelectionKeys();
  if (keys.size() == 1 && keys.front() == clickedKey) {
    RestoreSnapshotSelection();
    timer.StartOnce(DoubleClickIntervalMs());
    return true;
  }

  if (keys != snapshotKeys)
    Cancel();
  return false;
}

// Cancels deferred single-selection commit when activation matches the pending cell.
void DataViewDeferredSelectionGuard::NotifyItemActivated(
    const wxDataViewItem &item, int modelColumn) {
  if (!pending)
    return;

  if (itemKeyProvider(item) == clickedKey && modelColumn == clickedModelColumn)
    Cancel();
  else
    Cancel();
}

// Cancels pending work when rows are reloaded, sorted, deleted, or cleared.
void DataViewDeferredSelectionGuard::NotifyContentChanged() { Cancel(); }

// Cancels pending work before drag range selection changes the table selection.
void DataViewDeferredSelectionGuard::NotifyDragStarted() { Cancel(); }

// Cancels pending work before right-click/context actions proceed.
void DataViewDeferredSelectionGuard::NotifyContextActionStarted() { Cancel(); }

// Binds low-level mouse hooks on the data-view main window when available.
void DataViewDeferredSelectionGuard::BindMouseEvents() {
  if (!table)
    return;
  mainWindow = table->GetMainWindow();
  wxWindow *target = mainWindow ? mainWindow : table;
  target->Bind(wxEVT_LEFT_DOWN, &DataViewDeferredSelectionGuard::OnLeftDown,
               this);
  target->Bind(wxEVT_RIGHT_DOWN, &DataViewDeferredSelectionGuard::OnRightDown,
               this);
}

// Removes mouse hooks from the data-view item area.
void DataViewDeferredSelectionGuard::UnbindMouseEvents() {
  if (!table)
    return;
  wxWindow *target = mainWindow ? mainWindow : table;
  if (!target)
    return;
  target->Unbind(wxEVT_LEFT_DOWN, &DataViewDeferredSelectionGuard::OnLeftDown,
                 this);
  target->Unbind(wxEVT_RIGHT_DOWN, &DataViewDeferredSelectionGuard::OnRightDown,
                 this);
}

// Detects selected-row clicks that may become double-click bulk edits.
void DataViewDeferredSelectionGuard::OnLeftDown(wxMouseEvent &event) {
  Cancel();
  if (!table || HasModifier(event)) {
    event.Skip();
    return;
  }

  wxDataViewItem item;
  wxDataViewColumn *column = nullptr;
  table->HitTest(ToTablePosition(dynamic_cast<wxWindow *>(event.GetEventObject()),
                                 event.GetPosition()),
                 item, column);
  if (!item.IsOk() || !column) {
    event.Skip();
    return;
  }

  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (selections.size() <= 1 || !table->IsSelected(item)) {
    event.Skip();
    return;
  }

  clickedKey = itemKeyProvider(item);
  if (clickedKey.empty()) {
    event.Skip();
    return;
  }

  snapshotKeys = CurrentSelectionKeys();
  clickedModelColumn = column->GetModelColumn();
  pending = snapshotKeys.size() > 1;
  event.Skip();
}

// Cancels pending left-click state before native right-click handling.
void DataViewDeferredSelectionGuard::OnRightDown(wxMouseEvent &event) {
  NotifyContextActionStarted();
  event.Skip();
}

// Commits a deferred selected-row single click after the double-click interval.
void DataViewDeferredSelectionGuard::OnTimer(wxTimerEvent &WXUNUSED(event)) {
  if (!pending)
    return;
  CommitSingleSelection();
}

// Restores the original multi-selection while suppressing transient events.
void DataViewDeferredSelectionGuard::RestoreSnapshotSelection() {
  if (!table)
    return;

  wxWindowUpdateLocker locker(table);
  wxEventBlocker blocker(table, wxEVT_DATAVIEW_SELECTION_CHANGED);
  restoringSelection = true;
  table->UnselectAll();
  for (const auto &key : snapshotKeys) {
    wxDataViewItem item = FindItem(key);
    if (item.IsOk())
      table->Select(item);
  }
  wxDataViewItem clickedItem = FindItem(clickedKey);
  if (clickedItem.IsOk())
    table->SetCurrentItem(clickedItem);
  restoringSelection = false;
  if (highlightCallback)
    highlightCallback();
}

// Selects only the clicked row and synchronizes table selection exactly once.
void DataViewDeferredSelectionGuard::CommitSingleSelection() {
  if (!table) {
    Cancel();
    return;
  }

  const ItemKey key = clickedKey;
  Cancel();

  wxDataViewItem item = FindItem(key);
  if (!item.IsOk())
    return;

  wxWindowUpdateLocker locker(table);
  wxEventBlocker blocker(table, wxEVT_DATAVIEW_SELECTION_CHANGED);
  table->UnselectAll();
  table->Select(item);
  table->SetCurrentItem(item);
  if (selectionSyncCallback)
    selectionSyncCallback();
}

// Converts a mouse position from an event source window to table coordinates.
wxPoint DataViewDeferredSelectionGuard::ToTablePosition(wxWindow *source,
                                                        const wxPoint &position) const {
  if (!source || source == table)
    return position;
  return table->ScreenToClient(source->ClientToScreen(position));
}

// Returns selected stable item keys in the current table selection order.
std::vector<DataViewDeferredSelectionGuard::ItemKey>
DataViewDeferredSelectionGuard::CurrentSelectionKeys() const {
  std::vector<ItemKey> keys;
  if (!table)
    return keys;

  wxDataViewItemArray selections;
  table->GetSelections(selections);
  keys.reserve(selections.size());
  for (const auto &item : selections) {
    ItemKey key = itemKeyProvider(item);
    if (!key.empty())
      keys.push_back(key);
  }
  return keys;
}

// Finds a table item from its stable key.
wxDataViewItem DataViewDeferredSelectionGuard::FindItem(const ItemKey &key) const {
  return itemFinder ? itemFinder(key) : wxDataViewItem();
}

// Checks whether native multi-selection modifiers are active.
bool DataViewDeferredSelectionGuard::HasModifier(const wxMouseEvent &event) const {
  return event.ShiftDown() || event.ControlDown() || event.CmdDown() ||
         event.AltDown();
}

// Returns the platform double-click interval with a conservative fallback.
int DataViewDeferredSelectionGuard::DoubleClickIntervalMs() const {
  const int interval = wxSystemSettings::GetMetric(wxSYS_DCLICK_MSEC);
  return interval > 0 ? interval : 500;
}

} // namespace gui
