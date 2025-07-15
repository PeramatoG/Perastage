#include "trusstablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"

TrussTablePanel::TrussTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

void TrussTablePanel::InitializeTable()
{
    table->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 150);
    table->AppendTextColumn("Fixture ID", wxDATAVIEW_CELL_INERT, 90);
    table->AppendTextColumn("Layer", wxDATAVIEW_CELL_INERT, 100);
    table->AppendTextColumn("GDTF", wxDATAVIEW_CELL_INERT, 180);
    table->AppendTextColumn("Position", wxDATAVIEW_CELL_INERT, 150);
    table->AppendTextColumn("Rotation", wxDATAVIEW_CELL_INERT, 150);
}

void TrussTablePanel::ReloadData()
{
    table->DeleteAllItems();
    const auto& trusses = ConfigManager::Get().GetScene().trusses;

    for (const auto& [uuid, truss] : trusses)
    {
        wxVector<wxVariant> row;

        wxString name = wxString::FromUTF8(truss.name);
        wxString fixtureID = wxString::Format("%d", truss.fixtureId);
        wxString layer = wxString::FromUTF8(truss.layer);
        wxString gdtf = wxString::FromUTF8(truss.gdtfSpec);

        auto posArr = truss.transform.o;
        wxString pos = wxString::Format("%.1f, %.1f, %.1f", posArr[0], posArr[1], posArr[2]);

        auto euler = MatrixUtils::MatrixToEuler(truss.transform);
        wxString rot = wxString::Format("%.1f\u00B0, %.1f\u00B0, %.1f\u00B0", euler[0], euler[1], euler[2]);

        row.push_back(name);
        row.push_back(fixtureID);
        row.push_back(layer);
        row.push_back(gdtf);
        row.push_back(pos);
        row.push_back(rot);

        table->AppendItem(row);
    }
}
