#pragma once
#include <wx/wx.h>
#include <wx/dataview.h>
#include <vector>
#include "../external/json.hpp"

struct GdtfEntry {
    std::string manufacturer;
    std::string fixture;
    std::string rid;
    std::string url;
    std::string modes;
    std::string creator;
    std::string uploader;
    std::string creationDate;
    std::string revision;
    std::string lastModified;
    std::string version;
    std::string rating;
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

    wxTextCtrl* manufacturerCtrl = nullptr;
    wxTextCtrl* fixtureCtrl = nullptr;
    wxButton* searchBtn = nullptr;
    wxDataViewListCtrl* resultTable = nullptr;
    std::vector<GdtfEntry> entries;
    std::vector<int> visible;
    int selectedIndex = -1;
};
