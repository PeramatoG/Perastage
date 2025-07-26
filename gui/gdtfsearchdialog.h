#pragma once
#include <wx/wx.h>
#include <vector>
#include "../external/json.hpp"

struct GdtfEntry {
    std::string manufacturer;
    std::string name;
    std::string id;
    std::string url;
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
    void OnDownload(wxCommandEvent& evt);

    wxTextCtrl* brandCtrl = nullptr;
    wxTextCtrl* modelCtrl = nullptr;
    wxListBox* resultList = nullptr;
    std::vector<GdtfEntry> entries;
    std::vector<int> visible;
    int selectedIndex = -1;
};
