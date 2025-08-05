#include "fixtureeditdialog.h"
#include "fixturetablepanel.h"
#include "gdtfloader.h"
#include "projectutils.h"
#include "viewer3dpanel.h"
#include <wx/filedlg.h>
#include <wx/filename.h>

FixtureEditDialog::FixtureEditDialog(FixtureTablePanel* p, int r)
    : wxDialog(p, wxID_ANY, "Edit Fixture", wxDefaultPosition, wxSize(450,600)),
      panel(p), row(r) {
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
    grid->AddGrowableCol(1, 1);

    auto* table = panel->table; // friend access
    ctrls.resize(panel->columnLabels.size(), nullptr);

    for (size_t i = 0; i < panel->columnLabels.size(); ++i) {
        grid->Add(new wxStaticText(this, wxID_ANY, panel->columnLabels[i]), 0, wxALIGN_CENTER_VERTICAL);
        wxVariant v; table->GetValue(v, row, i);
        if (i == 7) {
            wxString gdtfPath;
            if ((size_t)row < panel->gdtfPaths.size())
                gdtfPath = panel->gdtfPaths[row];
            modeChoice = new wxChoice(this, wxID_ANY);
            auto modes = GetGdtfModes(gdtfPath.ToStdString());
            for (const auto& m : modes)
                modeChoice->Append(wxString::FromUTF8(m));
            int sel = modeChoice->FindString(v.GetString());
            if (sel != wxNOT_FOUND) modeChoice->SetSelection(sel);
            ctrls[i] = modeChoice;
            grid->Add(modeChoice, 1, wxEXPAND);
        } else if (i == 8) {
            chCountCtrl = new wxTextCtrl(this, wxID_ANY, v.GetString(), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
            ctrls[i] = chCountCtrl;
            grid->Add(chCountCtrl, 1, wxEXPAND);
        } else if (i == 9) {
            wxBoxSizer* hs = new wxBoxSizer(wxHORIZONTAL);
            modelCtrl = new wxTextCtrl(this, wxID_ANY);
            if ((size_t)row < panel->gdtfPaths.size())
                modelCtrl->SetValue(panel->gdtfPaths[row]);
            hs->Add(modelCtrl, 1, wxEXPAND|wxRIGHT, 5);
            wxButton* browse = new wxButton(this, wxID_ANY, "...");
            hs->Add(browse, 0);
            browse->Bind(wxEVT_BUTTON, &FixtureEditDialog::OnBrowse, this);
            ctrls[i] = modelCtrl;
            grid->Add(hs, 1, wxEXPAND);
        } else {
            wxTextCtrl* tc = new wxTextCtrl(this, wxID_ANY, v.GetString());
            ctrls[i] = tc;
            grid->Add(tc, 1, wxEXPAND);
        }
    }

    topSizer->Add(grid, 0, wxALL|wxEXPAND, 10);

    channelList = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1,150), wxTE_MULTILINE|wxTE_READONLY);
    topSizer->Add(channelList, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, 10);

    wxStdDialogButtonSizer* btns = new wxStdDialogButtonSizer();
    btns->AddButton(new wxButton(this, wxID_APPLY));
    btns->AddButton(new wxButton(this, wxID_OK));
    btns->AddButton(new wxButton(this, wxID_CANCEL));
    btns->Realize();
    topSizer->Add(btns, 0, wxALL|wxEXPAND, 10);

    Bind(wxEVT_BUTTON, &FixtureEditDialog::OnApply, this, wxID_APPLY);
    Bind(wxEVT_BUTTON, &FixtureEditDialog::OnOk, this, wxID_OK);
    Bind(wxEVT_BUTTON, &FixtureEditDialog::OnCancel, this, wxID_CANCEL);
    if (modeChoice)
        modeChoice->Bind(wxEVT_CHOICE, &FixtureEditDialog::OnModeChanged, this);

    SetSizerAndFit(topSizer);
    UpdateChannels();
}

void FixtureEditDialog::OnBrowse(wxCommandEvent&) {
    wxString fixDir = wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
        return;
    wxString path = fdlg.GetPath();
    modelCtrl->SetValue(path);
    // update type/power/weight fields
    if (ctrls.size() > 2) {
        wxString typeName = wxString::FromUTF8(GetGdtfFixtureName(std::string(path.mb_str())));
        if (typeName.empty()) typeName = fdlg.GetFilename();
        static_cast<wxTextCtrl*>(ctrls[2])->SetValue(typeName);
        float w=0.f, p=0.f;
        GetGdtfProperties(std::string(path.mb_str()), w, p);
        if (ctrls.size() > 16)
            static_cast<wxTextCtrl*>(ctrls[16])->SetValue(wxString::Format("%.1f", p));
        if (ctrls.size() > 17)
            static_cast<wxTextCtrl*>(ctrls[17])->SetValue(wxString::Format("%.2f", w));
    }
    // repopulate modes
    if (modeChoice) {
        modeChoice->Clear();
        auto modes = GetGdtfModes(std::string(path.mb_str()));
        for (const auto& m : modes)
            modeChoice->Append(wxString::FromUTF8(m));
        if (!modeChoice->IsEmpty())
            modeChoice->SetSelection(0);
    }
    UpdateChannels();
}

void FixtureEditDialog::OnModeChanged(wxCommandEvent&) {
    UpdateChannels();
}

void FixtureEditDialog::UpdateChannels() {
    wxString gdtfPath = modelCtrl ? modelCtrl->GetValue() : wxString();
    wxString mode = modeChoice ? modeChoice->GetStringSelection() : wxString();
    if (gdtfPath.empty() || mode.empty()) {
        channelList->SetValue("");
        if (chCountCtrl) chCountCtrl->SetValue("");
        return;
    }
    auto channels = GetGdtfModeChannels(std::string(gdtfPath.mb_str()), std::string(mode.mb_str()));
    wxString msg;
    for (const auto& ch : channels) {
        wxString func = wxString::FromUTF8(ch.function);
        if (func.empty()) func = "-";
        msg += wxString::Format("%d: %s\n", ch.channel, func);
    }
    channelList->SetValue(msg);
    int chCount = GetGdtfModeChannelCount(std::string(gdtfPath.mb_str()), std::string(mode.mb_str()));
    if (chCountCtrl)
        chCountCtrl->SetValue(chCount >= 0 ? wxString::Format("%d", chCount) : wxString());
}

void FixtureEditDialog::OnApply(wxCommandEvent&) {
    ApplyChanges();
}

void FixtureEditDialog::OnOk(wxCommandEvent&) {
    ApplyChanges();
    EndModal(wxID_OK);
}

void FixtureEditDialog::OnCancel(wxCommandEvent&) {
    EndModal(wxID_CANCEL);
}

void FixtureEditDialog::ApplyChanges() {
    if (!panel) return;
    auto* table = panel->table;
    wxString gdtfPath = modelCtrl ? modelCtrl->GetValue() : wxString();
    for (size_t i = 0; i < ctrls.size(); ++i) {
        if (i == 7 && modeChoice) {
            table->SetValue(wxVariant(modeChoice->GetStringSelection()), row, i);
        } else if (i == 8 && chCountCtrl) {
            table->SetValue(wxVariant(chCountCtrl->GetValue()), row, i);
        } else if (i == 9 && modelCtrl) {
            wxFileName fn(gdtfPath);
            table->SetValue(wxVariant(fn.GetFullName()), row, i);
            if ((size_t)row >= panel->gdtfPaths.size()) panel->gdtfPaths.resize(row+1);
            panel->gdtfPaths[row] = gdtfPath;
        } else {
            wxTextCtrl* tc = wxDynamicCast(ctrls[i], wxTextCtrl);
            if (tc)
                table->SetValue(wxVariant(tc->GetValue()), row, i);
        }
    }
    panel->ApplyModeForGdtf(gdtfPath);
    panel->UpdateSceneData();
    panel->HighlightDuplicateFixtureIds();
    applied = true;
    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
}

