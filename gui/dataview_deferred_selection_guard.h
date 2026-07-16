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

#include <functional>
#include <string>
#include <vector>
#include <wx/dataview.h>
#include <wx/timer.h>
#include <wx/window.h>

namespace gui {

// Coordinates native data-view selection timing for direct multi-row cell edits.
class DataViewDeferredSelectionGuard {
public:
  using ItemKey = std::string;
  using ItemKeyProvider = std::function<ItemKey(const wxDataViewItem &)>;
  using ItemFinder = std::function<wxDataViewItem(const ItemKey &)>;
  using SelectionSyncCallback = std::function<void()>;
  using HighlightCallback = std::function<void()>;

  DataViewDeferredSelectionGuard(wxWindow *owner, wxDataViewCtrl *table,
                                 ItemKeyProvider itemKeyProvider,
                                 ItemFinder itemFinder,
                                 SelectionSyncCallback selectionSyncCallback,
                                 HighlightCallback highlightCallback);
  ~DataViewDeferredSelectionGuard();

  void Cancel();
  bool HandleSelectionChanged();
  void NotifyItemActivated(const wxDataViewItem &item, int modelColumn);
  void NotifyContentChanged();
  void NotifyDragStarted();
  void NotifyContextActionStarted();

private:
  void BindMouseEvents();
  void UnbindMouseEvents();
  void OnLeftDown(wxMouseEvent &event);
  void OnRightDown(wxMouseEvent &event);
  void OnTimer(wxTimerEvent &event);
  void RestoreSnapshotSelection();
  void CommitSingleSelection();
  wxPoint ToTablePosition(wxWindow *source, const wxPoint &position) const;
  std::vector<ItemKey> CurrentSelectionKeys() const;
  wxDataViewItem FindItem(const ItemKey &key) const;
  bool HasModifier(const wxMouseEvent &event) const;
  int DoubleClickIntervalMs() const;

  wxWindow *owner = nullptr;
  wxDataViewCtrl *table = nullptr;
  wxWindow *mainWindow = nullptr;
  ItemKeyProvider itemKeyProvider;
  ItemFinder itemFinder;
  SelectionSyncCallback selectionSyncCallback;
  HighlightCallback highlightCallback;
  wxTimer timer;
  bool pending = false;
  bool restoringSelection = false;
  ItemKey clickedKey;
  int clickedModelColumn = wxNOT_FOUND;
  std::vector<ItemKey> snapshotKeys;
};

} // namespace gui
