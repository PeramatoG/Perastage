#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <vector>
#include <string>

class FixtureTablePanel;

class FixtureEditDialog : public wxDialog {
public:
    FixtureEditDialog(FixtureTablePanel* panel, int row);
    bool WasApplied() const { return applied; }

private:
    void OnApply(wxCommandEvent& evt);
    void OnOk(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);
    void OnBrowse(wxCommandEvent& evt);
    void OnModeChanged(wxCommandEvent& evt);
    void UpdateChannels();
    void ApplyChanges();

    FixtureTablePanel* panel;
    int row;
    std::vector<wxControl*> ctrls;
    wxChoice* modeChoice = nullptr;
    wxTextCtrl* chCountCtrl = nullptr;
    wxTextCtrl* modelCtrl = nullptr;
    wxTextCtrl* channelList = nullptr;
    bool applied = false;
};

