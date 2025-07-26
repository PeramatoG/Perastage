#include "gdtfsearchdialog.h"
#include "consolepanel.h"

using json = nlohmann::json;

GdtfSearchDialog::GdtfSearchDialog(wxWindow* parent, const std::string& listData)
    : wxDialog(parent, wxID_ANY, "Search GDTF", wxDefaultPosition, wxSize(800,600))
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
    searchSizer->Add(new wxStaticText(this, wxID_ANY, "Brand:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);
    brandCtrl = new wxTextCtrl(this, wxID_ANY);
    searchSizer->Add(brandCtrl, 1, wxRIGHT, 10);
    searchSizer->Add(new wxStaticText(this, wxID_ANY, "Model:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);
    modelCtrl = new wxTextCtrl(this, wxID_ANY);
    searchSizer->Add(modelCtrl, 1);
    searchBtn = new wxButton(this, wxID_ANY, "Search");
    searchSizer->Add(searchBtn, 0, wxLEFT, 10);
    sizer->Add(searchSizer, 0, wxEXPAND | wxALL, 10);

    resultList = new wxListBox(this, wxID_ANY);
    sizer->Add(resultList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* downloadBtn = new wxButton(this, wxID_OK, "Download");
    wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");
    btnSizer->AddStretchSpacer(1);
    btnSizer->Add(downloadBtn, 0, wxRIGHT, 5);
    btnSizer->Add(cancelBtn, 0);
    sizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizerAndFit(sizer);

    brandCtrl->Bind(wxEVT_TEXT, &GdtfSearchDialog::OnText, this);
    modelCtrl->Bind(wxEVT_TEXT, &GdtfSearchDialog::OnText, this);
    searchBtn->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnSearch, this);
    downloadBtn->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnDownload, this);
    resultList->Bind(wxEVT_LISTBOX_DCLICK, &GdtfSearchDialog::OnDownload, this);

    ParseList(listData);
    UpdateResults();
}

void GdtfSearchDialog::ParseList(const std::string& listData)
{
    entries.clear();
    if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Parse list: %zu bytes", listData.size());
        ConsolePanel::Instance()->AppendMessage(msg);
    }
    try {
        json j = json::parse(listData);
        if (j.is_object() && j.contains("data"))
            j = j["data"];
        if (!j.is_array())
            return;
        for (const auto& item : j) {
            GdtfEntry e;
            e.manufacturer = item.value("manufacturer", item.value("brand", item.value("mfr", "")));
            e.name = item.value("name", item.value("model", item.value("fixture", "")));
            e.id = item.value("id", item.value("fixtureId", item.value("uuid", "")));
            e.url = item.value("url", item.value("download", item.value("downloadUrl", "")));
            e.modes = item.value("modes", item.value("mode", item.value("modeCount", "")));
            e.creator = item.value("creator", item.value("user", item.value("userName", "")));
            e.tags = item.value("tags", "");
            e.dateAdded = item.value("dateAdded", item.value("created", item.value("uploadDate", "")));
            entries.push_back(e);
        }
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("Parsed %zu entries", entries.size());
            ConsolePanel::Instance()->AppendMessage(msg);
        }
    } catch(...) {
        // ignore parse errors
    }
}

void GdtfSearchDialog::UpdateResults()
{
    resultList->Clear();
    visible.clear();

    auto normalize = [](wxString s) {
        s = s.Lower();
        s.Replace(" ", "");
        s.Replace("-", "");
        return s;
    };

    wxString b = normalize(brandCtrl->GetValue());
    wxString m = normalize(modelCtrl->GetValue());
    if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Filtering brand='%s' model='%s'", b, m);
        ConsolePanel::Instance()->AppendMessage(msg);
    }
    for (size_t i = 0; i < entries.size(); ++i) {
        wxString manu = normalize(wxString::FromUTF8(entries[i].manufacturer));
        wxString name = normalize(wxString::FromUTF8(entries[i].name));
        if ((!b.empty() && !manu.Contains(b)) || (!m.empty() && !name.Contains(m)))
            continue;
        visible.push_back(static_cast<int>(i));
        wxString line = manu + " - " + name;
        if (!entries[i].modes.empty())
            line += " - " + wxString::FromUTF8(entries[i].modes);
        if (!entries[i].creator.empty())
            line += " - " + wxString::FromUTF8(entries[i].creator);
        if (!entries[i].tags.empty())
            line += " - " + wxString::FromUTF8(entries[i].tags);
        if (!entries[i].dateAdded.empty())
            line += " - " + wxString::FromUTF8(entries[i].dateAdded);
        resultList->Append(line);
    }
    if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Visible results: %zu", visible.size());
        ConsolePanel::Instance()->AppendMessage(msg);
    }
}

void GdtfSearchDialog::OnText(wxCommandEvent& WXUNUSED(evt))
{
    UpdateResults();
}

void GdtfSearchDialog::OnSearch(wxCommandEvent& WXUNUSED(evt))
{
    if (ConsolePanel::Instance())
        ConsolePanel::Instance()->AppendMessage("Search button pressed");
    UpdateResults();
}

void GdtfSearchDialog::OnDownload(wxCommandEvent& WXUNUSED(evt))
{
    int sel = resultList->GetSelection();
    if (sel != wxNOT_FOUND && sel < static_cast<int>(visible.size())) {
        selectedIndex = visible[sel];
        EndModal(wxID_OK);
    }
}

std::string GdtfSearchDialog::GetSelectedId() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
        return entries[selectedIndex].id;
    return {};
}

std::string GdtfSearchDialog::GetSelectedUrl() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
        return entries[selectedIndex].url;
    return {};
}

std::string GdtfSearchDialog::GetSelectedName() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
        return entries[selectedIndex].name;
    return {};
}
