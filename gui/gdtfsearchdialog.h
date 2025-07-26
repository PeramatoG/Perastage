#pragma once
#include <wx/wx.h>
#include <vector>
#include "../external/json.hpp"

struct GdtfEntry {
    std::string manufacturer;
    std::string name;
    std::string id;
    std::string url;
    std::string modes;
    std::string creator;
    std::string tags;
    std::string dateAdded;
};

class GdtfSearchDialog : public wxDialog {
public:
    GdtfSearchDialog(wxWindow* parent, const std::string& listData);
    std::string GetSelectedId() const;
    std::string GetSelectedUrl() const;
    std::string GetSelectedName() const;
private:
    void ParseList(const std::string& listData);
    void UpdateResults();
    void OnText(wxCommandEvent& evt);
    void OnSearch(wxCommandEvent& evt);
    void OnDownload(wxCommandEvent& evt);

    wxTextCtrl* brandCtrl = nullptr;
    wxTextCtrl* modelCtrl = nullptr;
    wxButton* searchBtn = nullptr;
    wxListBox* resultList = nullptr;
    std::vector<GdtfEntry> entries;
    std::vector<int> visible;
    int selectedIndex = -1;
};
