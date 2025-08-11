#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>

// Panel that shows a summary count of items by type/model/name
class SummaryPanel : public wxPanel {
public:
    explicit SummaryPanel(wxWindow* parent);

    void ShowFixtureSummary();
    void ShowTrussSummary();
    void ShowSceneObjectSummary();

    static SummaryPanel* Instance();
    static void SetInstance(SummaryPanel* panel);

private:
    wxDataViewListCtrl* table = nullptr;
    void ShowSummary(const std::vector<std::pair<std::string,int>>& items);
};
