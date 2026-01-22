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
#include <vector>
#include <wx/dataview.h>

class ColorfulDataViewListStore : public wxDataViewListStore {
public:
  std::vector<wxDataViewItemAttr> rowAttrs;
  std::vector<std::vector<wxDataViewItemAttr>> cellAttrs;
  std::vector<bool> selectionRows;
  wxColour selectionBackground;
  wxColour selectionForeground;
  bool selectionBackgroundEnabled = false;
  bool selectionForegroundEnabled = false;

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

    bool isSelected =
        row < selectionRows.size() && selectionRows[row] &&
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

    return hasAttr;
  }

  void AppendItem(const wxVector<wxVariant> &values, wxUIntPtr data = 0) {
    wxDataViewListStore::AppendItem(values, data);
    rowAttrs.emplace_back();
    cellAttrs.emplace_back();
    selectionRows.push_back(false);
  }

  void PrependItem(const wxVector<wxVariant> &values, wxUIntPtr data = 0) {
    wxDataViewListStore::PrependItem(values, data);
    rowAttrs.insert(rowAttrs.begin(), wxDataViewItemAttr());
    cellAttrs.insert(cellAttrs.begin(), std::vector<wxDataViewItemAttr>());
    selectionRows.insert(selectionRows.begin(), false);
  }

  void InsertItem(unsigned row, const wxVector<wxVariant> &values,
                  wxUIntPtr data = 0) {
    wxDataViewListStore::InsertItem(row, values, data);
    rowAttrs.insert(rowAttrs.begin() + row, wxDataViewItemAttr());
    cellAttrs.insert(cellAttrs.begin() + row,
                     std::vector<wxDataViewItemAttr>());
    selectionRows.insert(selectionRows.begin() + row, false);
  }

  void DeleteItem(unsigned row) {
    wxDataViewListStore::DeleteItem(row);
    if (row < rowAttrs.size())
      rowAttrs.erase(rowAttrs.begin() + row);
    if (row < cellAttrs.size())
      cellAttrs.erase(cellAttrs.begin() + row);
    if (row < selectionRows.size())
      selectionRows.erase(selectionRows.begin() + row);
  }

  void DeleteAllItems() {
    wxDataViewListStore::DeleteAllItems();
    rowAttrs.clear();
    cellAttrs.clear();
    selectionRows.clear();
  }

  void SetRowBackgroundColour(unsigned row, const wxColour &colour) {
    if (row >= rowAttrs.size())
      rowAttrs.resize(row + 1);
    rowAttrs[row].SetBackgroundColour(colour);
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
      RowChanged(row);
    }
  }

  void SetRowTextColour(unsigned row, const wxColour &colour) {
    if (row >= rowAttrs.size())
      rowAttrs.resize(row + 1);
    rowAttrs[row].SetColour(colour);
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
      RowChanged(row);
    }
  }

  void SetCellTextColour(unsigned row, unsigned col, const wxColour &colour) {
    if (row >= cellAttrs.size())
      cellAttrs.resize(row + 1);
    if (col >= cellAttrs[row].size())
      cellAttrs[row].resize(col + 1);
    cellAttrs[row][col].SetColour(colour);
    RowChanged(row);
  }

  void ClearCellTextColour(unsigned row, unsigned col) {
    if (row >= cellAttrs.size() || col >= cellAttrs[row].size())
      return;
    cellAttrs[row][col] = wxDataViewItemAttr();
    RowChanged(row);
  }

  void SetSelectionColours(const wxColour &background,
                           const wxColour &foreground) {
    selectionBackground = background;
    selectionForeground = foreground;
    selectionBackgroundEnabled = true;
    selectionForegroundEnabled = true;
  }

  void SetSelectedRows(const std::vector<bool> &rows) {
    std::vector<bool> oldRows = selectionRows;
    size_t oldSize = oldRows.size();
    selectionRows.assign(rows.begin(), rows.end());
    if (rowAttrs.size() > selectionRows.size())
      selectionRows.resize(rowAttrs.size(), false);
    size_t notifyCount = (std::max)(oldSize, selectionRows.size());
    for (size_t i = 0; i < notifyCount; ++i) {
      bool oldVal = i < oldSize ? oldRows[i] : false;
      bool newVal = i < selectionRows.size() ? selectionRows[i] : false;
      if (oldVal != newVal)
        RowChanged(i);
    }
  }

  int Compare(const wxDataViewItem &item1, const wxDataViewItem &item2,
              unsigned int column, bool ascending) const override {
    if (column == 1) {
      wxVariant v1, v2;
      const_cast<ColorfulDataViewListStore *>(this)->GetValue(v1, item1,
                                                              column);
      const_cast<ColorfulDataViewListStore *>(this)->GetValue(v2, item2,
                                                              column);
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
      int res;
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
      return ascending ? res : -res;
    }
    return wxDataViewListStore::Compare(item1, item2, column, ascending);
  }
};
