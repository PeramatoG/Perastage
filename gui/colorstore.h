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
#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>
#include <wx/dataview.h>

class ColorfulDataViewListStore : public wxDataViewListStore {
public:
  std::vector<wxDataViewItemAttr> rowAttrs;
  std::vector<std::vector<wxDataViewItemAttr>> cellAttrs;
  std::vector<bool> selectionRows;
  std::vector<bool> primaryHighlightRows;
  std::vector<bool> secondaryHighlightRows;
  std::unordered_set<wxUIntPtr> selectedItemKeys;
  std::unordered_set<wxUIntPtr> primaryHighlightItemKeys;
  std::unordered_set<wxUIntPtr> secondaryHighlightItemKeys;
  wxColour selectionBackground;
  wxColour selectionForeground;
  wxColour primaryHighlightBackground;
  wxColour secondaryHighlightBackground;
  wxColour highlightForeground;
  bool selectionBackgroundEnabled = false;
  bool selectionForegroundEnabled = false;
  bool highlightBackgroundEnabled = false;
  bool highlightForegroundEnabled = false;

  // Reports the schema registered in the list store instead of the base default.
  unsigned int GetColumnCount() const override { return m_cols.size(); }

  // Returns a registered column type without indexing outside the schema.
  wxString GetColumnType(unsigned int column) const override {
    return column < m_cols.size() ? m_cols[column] : wxString();
  }

  // Finds a model-owned row without trusting a transient item identifier.
  std::optional<unsigned> FindOwnedRow(const wxDataViewItem &item) const {
    if (!item.IsOk())
      return std::nullopt;
    const unsigned count = GetItemCount();
    for (unsigned row = 0; row < count; ++row) {
      if (GetItem(row) == item)
        return row;
    }
    return std::nullopt;
  }

  // Returns a cell value only when both item and model column are valid.
  void GetValue(wxVariant &value, const wxDataViewItem &item,
                unsigned int column) const override {
    const auto row = FindOwnedRow(item);
    if (!row || column >= GetColumnCount()) {
      value.MakeNull();
      return;
    }
    wxDataViewListStore::GetValueByRow(value, *row, column);
  }

  // Updates a cell only when both item and model column are valid.
  bool SetValue(const wxVariant &value, const wxDataViewItem &item,
                unsigned int column) override {
    const auto row = FindOwnedRow(item);
    return row && column < GetColumnCount()
               ? wxDataViewListStore::SetValueByRow(value, *row, column)
               : false;
  }

  bool GetAttrByRow(unsigned row, unsigned col,
                    wxDataViewItemAttr &attr) const override {
    bool hasAttr = false;
    bool hasTextColour = false;
    if (row < cellAttrs.size() && col < cellAttrs[row].size() &&
        !cellAttrs[row][col].IsDefault()) {
      attr = cellAttrs[row][col];
      hasAttr = true;
      hasTextColour = attr.HasColour();
    }

    if (!hasAttr && row < rowAttrs.size() && !rowAttrs[row].IsDefault()) {
      attr = rowAttrs[row];
      hasAttr = true;
      hasTextColour = attr.HasColour();
    }

    if (row >= GetItemCount())
      return hasAttr;
    const wxDataViewItem rowItem = GetItem(row);
    const wxUIntPtr itemKey = rowItem.IsOk() ? GetItemData(rowItem) : 0;
    bool isSelected =
        ((itemKey != 0 && selectedItemKeys.contains(itemKey)) ||
         (row < selectionRows.size() && selectionRows[row])) &&
        (selectionBackgroundEnabled || selectionForegroundEnabled);
    if (isSelected) {
      if (!hasAttr)
        attr = wxDataViewItemAttr();
      if (selectionBackgroundEnabled && !attr.HasBackgroundColour())
        attr.SetBackgroundColour(selectionBackground);
      if (selectionForegroundEnabled && !hasTextColour)
        attr.SetColour(selectionForeground);
      return true;
    }

    const bool isPrimaryHighlight =
        (itemKey != 0 && primaryHighlightItemKeys.contains(itemKey)) ||
        (row < primaryHighlightRows.size() && primaryHighlightRows[row]);
    const bool isSecondaryHighlight =
        (itemKey != 0 && secondaryHighlightItemKeys.contains(itemKey)) ||
        (row < secondaryHighlightRows.size() && secondaryHighlightRows[row]);
    if ((isPrimaryHighlight || isSecondaryHighlight) &&
        (highlightBackgroundEnabled || highlightForegroundEnabled)) {
      if (!hasAttr)
        attr = wxDataViewItemAttr();
      if (highlightBackgroundEnabled && !attr.HasBackgroundColour()) {
        attr.SetBackgroundColour(isPrimaryHighlight
                                     ? primaryHighlightBackground
                                     : secondaryHighlightBackground);
      }
      if (highlightForegroundEnabled && !hasTextColour)
        attr.SetColour(highlightForeground);
      return true;
    }

    return hasAttr;
  }

  void AppendItem(const wxVector<wxVariant> &values, wxUIntPtr data = 0) {
    wxDataViewListStore::AppendItem(values, data);
    rowAttrs.emplace_back();
    cellAttrs.emplace_back();
    selectionRows.push_back(false);
    primaryHighlightRows.push_back(false);
    secondaryHighlightRows.push_back(false);
  }

  void PrependItem(const wxVector<wxVariant> &values, wxUIntPtr data = 0) {
    wxDataViewListStore::PrependItem(values, data);
    rowAttrs.insert(rowAttrs.begin(), wxDataViewItemAttr());
    cellAttrs.insert(cellAttrs.begin(), std::vector<wxDataViewItemAttr>());
    selectionRows.insert(selectionRows.begin(), false);
    primaryHighlightRows.insert(primaryHighlightRows.begin(), false);
    secondaryHighlightRows.insert(secondaryHighlightRows.begin(), false);
  }

  void InsertItem(unsigned row, const wxVector<wxVariant> &values,
                  wxUIntPtr data = 0) {
    wxDataViewListStore::InsertItem(row, values, data);
    rowAttrs.insert(rowAttrs.begin() + row, wxDataViewItemAttr());
    cellAttrs.insert(cellAttrs.begin() + row,
                     std::vector<wxDataViewItemAttr>());
    selectionRows.insert(selectionRows.begin() + row, false);
    primaryHighlightRows.insert(primaryHighlightRows.begin() + row, false);
    secondaryHighlightRows.insert(secondaryHighlightRows.begin() + row, false);
  }

  void DeleteItem(unsigned row) {
    wxDataViewListStore::DeleteItem(row);
    if (row < rowAttrs.size())
      rowAttrs.erase(rowAttrs.begin() + row);
    if (row < cellAttrs.size())
      cellAttrs.erase(cellAttrs.begin() + row);
    if (row < selectionRows.size())
      selectionRows.erase(selectionRows.begin() + row);
    if (row < primaryHighlightRows.size())
      primaryHighlightRows.erase(primaryHighlightRows.begin() + row);
    if (row < secondaryHighlightRows.size())
      secondaryHighlightRows.erase(secondaryHighlightRows.begin() + row);
  }

  void DeleteAllItems() {
    wxDataViewListStore::DeleteAllItems();
    rowAttrs.clear();
    cellAttrs.clear();
    selectionRows.clear();
    primaryHighlightRows.clear();
    secondaryHighlightRows.clear();
    selectedItemKeys.clear();
    primaryHighlightItemKeys.clear();
    secondaryHighlightItemKeys.clear();
  }

  void SetRowBackgroundColour(unsigned row, const wxColour &colour) {
    if (row >= rowAttrs.size())
      rowAttrs.resize(row + 1);
    rowAttrs[row].SetBackgroundColour(colour);
    if (row < GetItemCount())
      RowChanged(row);
  }

  void ClearRowBackground(unsigned row) {
    if (row < rowAttrs.size()) {
      wxColour fg;
      bool hasFg = rowAttrs[row].HasColour();
      if (hasFg)
        fg = rowAttrs[row].GetColour();
      rowAttrs[row] = wxDataViewItemAttr();
      if (hasFg)
        rowAttrs[row].SetColour(fg);
      if (row < GetItemCount())
        RowChanged(row);
    }
  }

  void SetRowTextColour(unsigned row, const wxColour &colour) {
    if (row >= rowAttrs.size())
      rowAttrs.resize(row + 1);
    rowAttrs[row].SetColour(colour);
    if (row < GetItemCount())
      RowChanged(row);
  }

  void ClearRowTextColour(unsigned row) {
    if (row < rowAttrs.size()) {
      wxColour bg;
      bool hasBg = rowAttrs[row].HasBackgroundColour();
      if (hasBg)
        bg = rowAttrs[row].GetBackgroundColour();
      rowAttrs[row] = wxDataViewItemAttr();
      if (hasBg)
        rowAttrs[row].SetBackgroundColour(bg);
      if (row < GetItemCount())
        RowChanged(row);
    }
  }

  void SetCellTextColour(unsigned row, unsigned col, const wxColour &colour,
                         bool notify = true) {
    if (row >= cellAttrs.size())
      cellAttrs.resize(row + 1);
    if (col >= cellAttrs[row].size())
      cellAttrs[row].resize(col + 1);
    cellAttrs[row][col].SetColour(colour);
    if (notify && row < GetItemCount())
      RowChanged(row);
  }

  void ClearCellTextColour(unsigned row, unsigned col, bool notify = true) {
    if (row >= cellAttrs.size() || col >= cellAttrs[row].size())
      return;
    cellAttrs[row][col] = wxDataViewItemAttr();
    if (notify && row < GetItemCount())
      RowChanged(row);
  }

  void SetSelectionColours(const wxColour &background,
                           const wxColour &foreground) {
    selectionBackground = background;
    selectionForeground = foreground;
    selectionBackgroundEnabled = true;
    selectionForegroundEnabled = true;
  }

  void SetSelectedItemKeys(const std::vector<wxUIntPtr> &itemKeys) {
    selectedItemKeys.clear();
    selectedItemKeys.insert(itemKeys.begin(), itemKeys.end());
    const size_t itemCount = GetItemCount();
    for (size_t i = 0; i < itemCount; ++i)
      RowChanged(i);
  }

  void SetSelectedRows(const std::vector<bool> &rows) {
    std::vector<bool> oldRows = selectionRows;
    size_t oldSize = oldRows.size();
    selectionRows.assign(rows.begin(), rows.end());
    size_t itemCount = GetItemCount();
    if (itemCount > selectionRows.size())
      selectionRows.resize(itemCount, false);
    if (selectionRows.size() > itemCount)
      selectionRows.resize(itemCount);
    size_t notifyCount =
        (std::min)((std::max)(oldSize, selectionRows.size()), itemCount);
    for (size_t i = 0; i < notifyCount; ++i) {
      bool oldVal = i < oldSize ? oldRows[i] : false;
      bool newVal = i < selectionRows.size() ? selectionRows[i] : false;
      if (oldVal != newVal)
        RowChanged(i);
    }
  }

  // Applies primary and secondary item-key highlights using selection-style colors.
  void SetHighlightItemKeys(const std::vector<wxUIntPtr> &primaryKeys,
                            const std::vector<wxUIntPtr> &secondaryKeys,
                            const wxColour &primaryBackground,
                            const wxColour &secondaryBackground,
                            const wxColour &foreground) {
    primaryHighlightItemKeys.clear();
    secondaryHighlightItemKeys.clear();
    primaryHighlightItemKeys.insert(primaryKeys.begin(), primaryKeys.end());
    secondaryHighlightItemKeys.insert(secondaryKeys.begin(), secondaryKeys.end());
    primaryHighlightBackground = primaryBackground;
    secondaryHighlightBackground = secondaryBackground;
    highlightForeground = foreground;
    highlightBackgroundEnabled = true;
    highlightForegroundEnabled = true;
    const size_t itemCount = GetItemCount();
    for (size_t i = 0; i < itemCount; ++i)
      RowChanged(i);
  }

  // Applies primary and secondary row highlights using selection-style colors.
  void SetHighlightRows(const std::vector<bool> &primaryRows,
                        const std::vector<bool> &secondaryRows,
                        const wxColour &primaryBackground,
                        const wxColour &secondaryBackground,
                        const wxColour &foreground) {
    std::vector<bool> oldPrimaryRows = primaryHighlightRows;
    std::vector<bool> oldSecondaryRows = secondaryHighlightRows;
    primaryHighlightRows.assign(primaryRows.begin(), primaryRows.end());
    secondaryHighlightRows.assign(secondaryRows.begin(), secondaryRows.end());
    size_t itemCount = GetItemCount();
    if (itemCount > primaryHighlightRows.size())
      primaryHighlightRows.resize(itemCount, false);
    if (itemCount > secondaryHighlightRows.size())
      secondaryHighlightRows.resize(itemCount, false);
    if (primaryHighlightRows.size() > itemCount)
      primaryHighlightRows.resize(itemCount);
    if (secondaryHighlightRows.size() > itemCount)
      secondaryHighlightRows.resize(itemCount);
    primaryHighlightBackground = primaryBackground;
    secondaryHighlightBackground = secondaryBackground;
    highlightForeground = foreground;
    highlightBackgroundEnabled = true;
    highlightForegroundEnabled = true;

    size_t notifyCount =
        (std::min)((std::max)(oldPrimaryRows.size(), primaryHighlightRows.size()),
                   itemCount);
    notifyCount = (std::max)(notifyCount,
                             (std::min)((std::max)(oldSecondaryRows.size(),
                                                    secondaryHighlightRows.size()),
                                        itemCount));
    for (size_t i = 0; i < notifyCount; ++i) {
      bool oldPrimary = i < oldPrimaryRows.size() ? oldPrimaryRows[i] : false;
      bool newPrimary =
          i < primaryHighlightRows.size() ? primaryHighlightRows[i] : false;
      bool oldSecondary =
          i < oldSecondaryRows.size() ? oldSecondaryRows[i] : false;
      bool newSecondary =
          i < secondaryHighlightRows.size() ? secondaryHighlightRows[i] : false;
      if (oldPrimary != newPrimary || oldSecondary != newSecondary)
        RowChanged(i);
    }
  }

  int Compare(const wxDataViewItem &item1, const wxDataViewItem &item2,
              unsigned int column, bool ascending) const override {
    const auto row1 = FindOwnedRow(item1);
    const auto row2 = FindOwnedRow(item2);
    if (!row1 || !row2 || column >= GetColumnCount()) {
      const auto key1 = reinterpret_cast<std::uintptr_t>(item1.GetID());
      const auto key2 = reinterpret_cast<std::uintptr_t>(item2.GetID());
      const int fallback = key1 < key2 ? -1 : (key1 > key2 ? 1 : 0);
      return ascending ? fallback : -fallback;
    }
    int res = 0;
    if (column == 1) {
      wxVariant v1, v2;
      wxDataViewListStore::GetValueByRow(v1, *row1, column);
      wxDataViewListStore::GetValueByRow(v2, *row2, column);
      wxString s1 = v1.GetString();
      wxString s2 = v2.GetString();

      auto parse = [](const wxString &s, wxString &prefix, long &num) {
        int pos = s.find_last_of(' ');
        if (pos != wxNOT_FOUND && s.Mid(pos + 1).ToLong(&num)) {
          prefix = s.Left(pos);
          return true;
        }
        return false;
      };

      wxString p1, p2;
      long n1 = 0, n2 = 0;
      bool ok1 = parse(s1, p1, n1);
      bool ok2 = parse(s2, p2, n2);
      if (ok1 && ok2 && p1 == p2) {
        if (n1 < n2)
          res = -1;
        else if (n1 > n2)
          res = 1;
        else
          res = 0;
      } else {
        res = s1.Cmp(s2);
      }
    } else {
      wxVariant v1, v2;
      wxDataViewListStore::GetValueByRow(v1, *row1, column);
      wxDataViewListStore::GetValueByRow(v2, *row2, column);
      res = v1.GetString().Cmp(v2.GetString());
      if (v1.GetType() == "bool" && v2.GetType() == "bool")
        res = static_cast<int>(v1.GetBool()) - static_cast<int>(v2.GetBool());
    }

    if (res == 0) {
      const wxUIntPtr key1 = GetItemData(item1);
      const wxUIntPtr key2 = GetItemData(item2);
      if (key1 < key2)
        res = -1;
      else if (key1 > key2)
        res = 1;
    }

    return ascending ? res : -res;
  }
};
