#include "sceneobjecttablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include <algorithm>

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
    columnLabels = {"Name", "Layer", "Model File", "Position", "Rotation"};
    std::vector<int> widths = {150, 100, 180, 150, 150};
    for (size_t i = 0; i < columnLabels.size(); ++i)
        table->AppendTextColumn(
            columnLabels[i], wxDATAVIEW_CELL_INERT, widths[i], wxALIGN_LEFT,
            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
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

    // Let wxDataViewListCtrl manage column headers and sorting
}
