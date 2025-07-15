#include "sceneobjecttablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"

SceneObjectTablePanel::SceneObjectTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

void SceneObjectTablePanel::InitializeTable()
{
    table->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 150);
    table->AppendTextColumn("Layer", wxDATAVIEW_CELL_INERT, 100);
    table->AppendTextColumn("Model File", wxDATAVIEW_CELL_INERT, 180);
    table->AppendTextColumn("Position", wxDATAVIEW_CELL_INERT, 150);
    table->AppendTextColumn("Rotation", wxDATAVIEW_CELL_INERT, 150);
}

void SceneObjectTablePanel::ReloadData()
{
    table->DeleteAllItems();
    const auto& objs = ConfigManager::Get().GetScene().sceneObjects;

    for (const auto& [uuid, obj] : objs)
    {
        wxVector<wxVariant> row;

        wxString name = wxString::FromUTF8(obj.name);
        wxString layer = wxString::FromUTF8(obj.layer);
        wxString model = wxString::FromUTF8(obj.modelFile);

        auto posArr = obj.transform.o;
        wxString pos = wxString::Format("%.1f, %.1f, %.1f", posArr[0], posArr[1], posArr[2]);

        auto euler = MatrixUtils::MatrixToEuler(obj.transform);
        wxString rot = wxString::Format("%.1f\u00B0, %.1f\u00B0, %.1f\u00B0", euler[0], euler[1], euler[2]);

        row.push_back(name);
        row.push_back(layer);
        row.push_back(model);
        row.push_back(pos);
        row.push_back(rot);

        table->AppendItem(row);
    }
}
