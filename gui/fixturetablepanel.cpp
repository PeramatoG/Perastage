#include "fixturetablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include <algorithm>

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
    for (size_t i = 0; i < columnLabels.size(); ++i)
        table->AppendTextColumn(
            columnLabels[i], wxDATAVIEW_CELL_INERT, widths[i], wxALIGN_LEFT,
            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
}

void FixtureTablePanel::ReloadData()
{
    table->DeleteAllItems();

    const auto& fixtures = ConfigManager::Get().GetScene().fixtures;
    for (const auto& [uuid, fixture] : fixtures)
    {
        wxVector<wxVariant> row;

        wxString name = wxString::FromUTF8(fixture.name);
        wxString fixtureID = wxString::Format("%d", fixture.fixtureId);
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

