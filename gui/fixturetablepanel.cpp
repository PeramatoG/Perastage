#include "fixturetablepanel.h"
#include "configmanager.h"

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
    table->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 150);
    table->AppendTextColumn("Fixture ID", wxDATAVIEW_CELL_INERT, 90);
    table->AppendTextColumn("Layer", wxDATAVIEW_CELL_INERT, 100);
    table->AppendTextColumn("Address (Univ.Ch)", wxDATAVIEW_CELL_INERT, 120);
    table->AppendTextColumn("GDTF", wxDATAVIEW_CELL_INERT, 180);
    table->AppendTextColumn("Position", wxDATAVIEW_CELL_INERT, 150);
    table->AppendTextColumn("Rotation", wxDATAVIEW_CELL_INERT, 150);
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

        wxString pos = wxString::FromUTF8(fixture.position); // Currently a string in struct
        wxString rot = wxString("0.0°, 0.0°, 0.0°"); // Placeholder until we extract from matrix

        row.push_back(name);
        row.push_back(fixtureID);
        row.push_back(layer);
        row.push_back(address);
        row.push_back(gdtf);
        row.push_back(pos);
        row.push_back(rot);

        table->AppendItem(row);
    }
}

