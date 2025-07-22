#include "fixturetablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include <algorithm>
#include <wx/tokenzr.h>

wxBEGIN_EVENT_TABLE(FixtureTablePanel, wxPanel)
    EVT_DATAVIEW_COLUMN_HEADER_CLICK(wxID_ANY, FixtureTablePanel::OnColumnHeaderClick)
wxEND_EVENT_TABLE()

FixtureTablePanel::FixtureTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

void FixtureTablePanel::InitializeTable()
{
    columnLabels = {
        "Name",
        "Fixture ID",
        "Layer",
        "Address (Univ.Ch)",
        "GDTF",
        "Position",
        "Hang Pos",
        "Rotation"
    };

    std::vector<int> widths = {150, 90, 100, 120, 180, 150, 120, 150};
    int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;

    // Column 0: Name (string)
    table->AppendTextColumn(columnLabels[0], wxDATAVIEW_CELL_INERT, widths[0],
                            wxALIGN_LEFT, flags);

    // Column 1: Fixture ID (numeric for proper sorting)
    auto* idRenderer =
        new wxDataViewTextRenderer("long", wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
    auto* idColumn = new wxDataViewColumn(columnLabels[1], idRenderer, 1,
                                          widths[1], wxALIGN_LEFT, flags);
    table->AppendColumn(idColumn);

    // Remaining columns as regular text
    for (size_t i = 2; i < columnLabels.size(); ++i)
        table->AppendTextColumn(columnLabels[i], wxDATAVIEW_CELL_INERT,
                                widths[i], wxALIGN_LEFT, flags);
}

void FixtureTablePanel::ReloadData()
{
    table->DeleteAllItems();

    const auto& fixtures = ConfigManager::Get().GetScene().fixtures;
    for (const auto& [uuid, fixture] : fixtures)
    {
        wxVector<wxVariant> row;

        wxString name = wxString::FromUTF8(fixture.name);
        long fixtureID = static_cast<long>(fixture.fixtureId);
        wxString layer = wxString::FromUTF8(fixture.layer);
        wxString address;
        if (fixture.address.empty())
            address = wxString::FromUTF8("–");
        else
            address = wxString::FromUTF8(fixture.address);
        wxString gdtf = wxString::FromUTF8(fixture.gdtfSpec);

        auto posArr = fixture.GetPosition();
        wxString pos = wxString::Format("%.1f, %.1f, %.1f", posArr[0], posArr[1], posArr[2]);
        wxString posName = wxString::FromUTF8(fixture.positionName);

        auto euler = MatrixUtils::MatrixToEuler(fixture.transform);
        wxString rot = wxString::Format("%.1f\u00B0, %.1f\u00B0, %.1f\u00B0", euler[0], euler[1], euler[2]);

        row.push_back(name);
        row.push_back(fixtureID);
        row.push_back(layer);
        row.push_back(address);
        row.push_back(gdtf);
        row.push_back(pos);
        row.push_back(posName);
        row.push_back(rot);

        table->AppendItem(row);
    }

    // Let wxDataViewListCtrl manage column headers and sorting
}

void FixtureTablePanel::OnColumnHeaderClick(wxDataViewEvent& event)
{
    if (event.GetColumn() == 3)
    {
        addrSortAscending = !addrSortAscending;
        SortByAddress(addrSortAscending);
        table->GetColumn(3)->SetSortOrder(addrSortAscending);
        // Prevent default sorting behaviour
        event.Veto();
    }
    else
    {
        // For other columns use default handling
        event.Skip();
    }
}

void FixtureTablePanel::SortByAddress(bool ascending)
{
    struct RowData { wxVector<wxVariant> values; };
    std::vector<RowData> rows;
    unsigned rowCount = table->GetItemCount();
    unsigned colCount = table->GetColumnCount();

    for (unsigned r = 0; r < rowCount; ++r)
    {
        RowData rd;
        for (unsigned c = 0; c < colCount; ++c)
        {
            wxVariant val;
            table->GetValue(val, r, c);
            rd.values.push_back(val);
        }
        rows.push_back(std::move(rd));
    }

    auto parseAddr = [](const wxString& s) {
        long u = 0, ch = 0;
        wxStringTokenizer tk(s, ".");
        if (tk.HasMoreTokens()) tk.GetNextToken().ToLong(&u);
        if (tk.HasMoreTokens()) tk.GetNextToken().ToLong(&ch);
        return std::make_pair(u, ch);
    };

    std::sort(rows.begin(), rows.end(), [&](const RowData& a, const RowData& b) {
        auto aa = parseAddr(a.values[3].GetString());
        auto bb = parseAddr(b.values[3].GetString());
        if (aa.first == bb.first)
            return ascending ? aa.second < bb.second : aa.second > bb.second;
        return ascending ? aa.first < bb.first : aa.first > bb.first;
    });

    table->DeleteAllItems();
    for (const auto& r : rows)
        table->AppendItem(r.values);
}

