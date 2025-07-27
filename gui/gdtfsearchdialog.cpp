#include "gdtfsearchdialog.h"
#include "consolepanel.h"
#include <wx/settings.h>

using json = nlohmann::json;

GdtfSearchDialog::GdtfSearchDialog(wxWindow* parent, const std::string& listData)
    : wxDialog(parent, wxID_ANY, "Search GDTF", wxDefaultPosition,
               wxSize(1000,700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
    searchSizer->Add(new wxStaticText(this, wxID_ANY, "Manufacturer:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);
    manufacturerCtrl = new wxTextCtrl(this, wxID_ANY);
    searchSizer->Add(manufacturerCtrl, 1, wxRIGHT, 10);
    searchSizer->Add(new wxStaticText(this, wxID_ANY, "Fixture:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);
    fixtureCtrl = new wxTextCtrl(this, wxID_ANY);
    searchSizer->Add(fixtureCtrl, 1);
    searchBtn = new wxButton(this, wxID_ANY, "Search");
    searchSizer->Add(searchBtn, 0, wxLEFT, 10);
    sizer->Add(searchSizer, 0, wxEXPAND | wxALL, 10);

    resultTable = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                         wxDefaultSize, wxDV_ROW_LINES);
#if defined(wxHAS_GENERIC_DATAVIEWCTRL)
    resultTable->SetAlternateRowColour(
        wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
#endif
    int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
    resultTable->AppendTextColumn("Manufacturer", wxDATAVIEW_CELL_INERT, 150,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Fixture", wxDATAVIEW_CELL_INERT, 200,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Modes", wxDATAVIEW_CELL_INERT, 60,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Creator", wxDATAVIEW_CELL_INERT, 120,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Uploader", wxDATAVIEW_CELL_INERT, 100,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Creation Date", wxDATAVIEW_CELL_INERT, 110,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Revision", wxDATAVIEW_CELL_INERT, 90,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Last Modified", wxDATAVIEW_CELL_INERT, 110,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Version", wxDATAVIEW_CELL_INERT, 80,
                                  wxALIGN_LEFT, flags);
    resultTable->AppendTextColumn("Rating", wxDATAVIEW_CELL_INERT, 60,
                                  wxALIGN_LEFT, flags);
    sizer->Add(resultTable, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* downloadBtn = new wxButton(this, wxID_OK, "Download");
    wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");
    btnSizer->AddStretchSpacer(1);
    btnSizer->Add(downloadBtn, 0, wxRIGHT, 5);
    btnSizer->Add(cancelBtn, 0);
    sizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(sizer);
    SetMinSize(wxSize(800, 600));
    SetSize(wxSize(1000, 700));

    manufacturerCtrl->Bind(wxEVT_TEXT, &GdtfSearchDialog::OnText, this);
    fixtureCtrl->Bind(wxEVT_TEXT, &GdtfSearchDialog::OnText, this);
    searchBtn->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnSearch, this);
    downloadBtn->Bind(wxEVT_BUTTON, &GdtfSearchDialog::OnDownload, this);
    resultTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                      &GdtfSearchDialog::OnDownload, this);

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
            e.fixture = getValue(item, {"fixture", "name", "model"});
            e.rid = getValue(item, {"rid", "revisionId"});
            e.url = getValue(item, {"url", "download", "downloadUrl"});
            e.modes = getValue(item, {"modes", "mode", "modeCount"});
            e.creator = getValue(item, {"creator", "user", "userName"});
            e.uploader = getValue(item, {"uploader"});
            e.creationDate = getValue(item, {"creationDate"});
            e.revision = getValue(item, {"revision"});
            e.lastModified = getValue(item, {"lastModified"});
            e.version = getValue(item, {"version"});
            e.rating = getValue(item, {"rating"});
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
    resultTable->DeleteAllItems();
    visible.clear();

    auto normalize = [](wxString s) {
        s = s.Lower();
        s.Replace(" ", "");
        s.Replace("-", "");
        return s;
    };

    wxString b = normalize(manufacturerCtrl->GetValue());
    wxString m = normalize(fixtureCtrl->GetValue());
    if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Filtering manufacturer='%s' fixture='%s'", b, m);
        ConsolePanel::Instance()->AppendMessage(msg);
    }
    for (size_t i = 0; i < entries.size(); ++i) {
        wxString manuOrig = wxString::FromUTF8(entries[i].manufacturer);
        wxString fixOrig = wxString::FromUTF8(entries[i].fixture);
        wxString manu = normalize(manuOrig);
        wxString fix = normalize(fixOrig);
        if ((!b.empty() && !manu.Contains(b)) || (!m.empty() && !fix.Contains(m)))
            continue;
        visible.push_back(static_cast<int>(i));
        wxVector<wxVariant> row;
        row.push_back(manuOrig);
        row.push_back(fixOrig);
        row.push_back(wxString::FromUTF8(entries[i].modes));
        row.push_back(wxString::FromUTF8(entries[i].creator));
        row.push_back(wxString::FromUTF8(entries[i].uploader));
        row.push_back(wxString::FromUTF8(entries[i].creationDate));
        row.push_back(wxString::FromUTF8(entries[i].revision));
        row.push_back(wxString::FromUTF8(entries[i].lastModified));
        row.push_back(wxString::FromUTF8(entries[i].version));
        row.push_back(wxString::FromUTF8(entries[i].rating));
        resultTable->AppendItem(row);
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
    wxDataViewItem item = resultTable->GetSelection();
    int row = resultTable->ItemToRow(item);
    if (row != wxNOT_FOUND && row < static_cast<int>(visible.size())) {
        selectedIndex = visible[row];
        EndModal(wxID_OK);
    }
}

std::string GdtfSearchDialog::GetSelectedId() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
        return entries[selectedIndex].rid;
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
        return entries[selectedIndex].fixture;
    return {};
}
