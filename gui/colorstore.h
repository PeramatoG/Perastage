#pragma once
#include <wx/dataview.h>
#include <vector>

class ColorfulDataViewListStore : public wxDataViewListStore {
public:
    std::vector<wxDataViewItemAttr> rowAttrs;

    bool GetAttrByRow(unsigned row, unsigned col, wxDataViewItemAttr& attr) const override {
        if (row < rowAttrs.size() && !rowAttrs[row].IsDefault()) {
            attr = rowAttrs[row];
            return true;
        }
        return false;
    }

    void AppendItem(const wxVector<wxVariant>& values, wxUIntPtr data = 0) {
        wxDataViewListStore::AppendItem(values, data);
        rowAttrs.emplace_back();
    }

    void PrependItem(const wxVector<wxVariant>& values, wxUIntPtr data = 0) {
        wxDataViewListStore::PrependItem(values, data);
        rowAttrs.insert(rowAttrs.begin(), wxDataViewItemAttr());
    }

    void InsertItem(unsigned row, const wxVector<wxVariant>& values, wxUIntPtr data = 0) {
        wxDataViewListStore::InsertItem(row, values, data);
        rowAttrs.insert(rowAttrs.begin() + row, wxDataViewItemAttr());
    }

    void DeleteItem(unsigned row) {
        wxDataViewListStore::DeleteItem(row);
        if (row < rowAttrs.size())
            rowAttrs.erase(rowAttrs.begin() + row);
    }

    void DeleteAllItems() {
        wxDataViewListStore::DeleteAllItems();
        rowAttrs.clear();
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
};
