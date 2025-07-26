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
        if (j.is_object()) {
            if (j.contains("data"))
                j = j["data"];
            if (j.contains("fixtures"))
                j = j["fixtures"];
            if (j.contains("list"))
                j = j["list"];
        }
        if (!j.is_array())
            return;
        auto jsonToString = [](const json& v) -> std::string {
            if (v.is_string())
                return v.get<std::string>();
            if (v.is_number())
                return v.dump();
            if (v.is_array()) {
                std::string result;
                for (size_t i = 0; i < v.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    const auto& el = v[i];
                    if (el.is_string())
                        result += el.get<std::string>();
                    else if (el.is_object() && el.contains("name") && el["name"].is_string())
                        result += el["name"].get<std::string>();
                    else
                        result += el.dump();
                }
                return result;
            }
            if (v.is_object())
                return v.dump();
            return {};
        };

        auto getValue = [&](const json& obj, std::initializer_list<const char*> keys) -> std::string {
            for (const char* k : keys) {
                auto it = obj.find(k);
                if (it != obj.end())
                    return jsonToString(*it);
            }
            return {};
        };

        for (const auto& item : j) {
            GdtfEntry e;
            e.manufacturer = getValue(item, {"manufacturer", "brand", "mfr"});
            e.name = getValue(item, {"name", "model", "fixture"});
            e.id = getValue(item, {"id", "fixtureId", "uuid"});
            e.url = getValue(item, {"url", "download", "downloadUrl"});
            e.modes = getValue(item, {"modes", "mode", "modeCount"});
            e.creator = getValue(item, {"creator", "user", "userName"});
            e.tags = getValue(item, {"tags"});
            e.dateAdded = getValue(item, {"dateAdded", "created", "uploadDate"});
            entries.push_back(e);
        }
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("Parsed %zu entries", entries.size());
            ConsolePanel::Instance()->AppendMessage(msg);
        }
    } catch(const std::exception& e) {
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("JSON parse error: %s", e.what());
            ConsolePanel::Instance()->AppendMessage(msg);
            wxString sample = wxString::FromUTF8(listData.substr(0, 200));
            ConsolePanel::Instance()->AppendMessage("Sample: " + sample);
        }
    } catch(...) {
        if (ConsolePanel::Instance())
            ConsolePanel::Instance()->AppendMessage("Unknown JSON parse error");
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
    // Do not update the results list until the user presses the Search button
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
