#pragma once
#include <wx/dataview.h>
#include <vector>

class ColorfulDataViewListStore : public wxDataViewListStore {
public:
    std::vector<wxDataViewItemAttr> rowAttrs;
    std::vector<std::vector<wxDataViewItemAttr>> cellAttrs;

    bool GetAttrByRow(unsigned row, unsigned col, wxDataViewItemAttr& attr) const override {
        if (row < cellAttrs.size() && col < cellAttrs[row].size() &&
            !cellAttrs[row][col].IsDefault()) {
            attr = cellAttrs[row][col];
            return true;
        }

        if (row < rowAttrs.size() && !rowAttrs[row].IsDefault()) {
            attr = rowAttrs[row];
            return true;
        }
        return false;
    }

    void AppendItem(const wxVector<wxVariant>& values, wxUIntPtr data = 0) {
        wxDataViewListStore::AppendItem(values, data);
        rowAttrs.emplace_back();
        cellAttrs.emplace_back();
    }

    void PrependItem(const wxVector<wxVariant>& values, wxUIntPtr data = 0) {
        wxDataViewListStore::PrependItem(values, data);
        rowAttrs.insert(rowAttrs.begin(), wxDataViewItemAttr());
        cellAttrs.insert(cellAttrs.begin(), std::vector<wxDataViewItemAttr>());
    }

    void InsertItem(unsigned row, const wxVector<wxVariant>& values, wxUIntPtr data = 0) {
        wxDataViewListStore::InsertItem(row, values, data);
        rowAttrs.insert(rowAttrs.begin() + row, wxDataViewItemAttr());
        cellAttrs.insert(cellAttrs.begin() + row, std::vector<wxDataViewItemAttr>());
    }

    void DeleteItem(unsigned row) {
        wxDataViewListStore::DeleteItem(row);
        if (row < rowAttrs.size())
            rowAttrs.erase(rowAttrs.begin() + row);
        if (row < cellAttrs.size())
            cellAttrs.erase(cellAttrs.begin() + row);
    }

    void DeleteAllItems() {
        wxDataViewListStore::DeleteAllItems();
        rowAttrs.clear();
        cellAttrs.clear();
    }

    void SetRowBackgroundColour(unsigned row, const wxColour& colour) {
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

    void SetRowTextColour(unsigned row, const wxColour& colour) {
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

    void SetCellTextColour(unsigned row, unsigned col, const wxColour& colour) {
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
};
