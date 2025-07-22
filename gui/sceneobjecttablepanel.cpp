#include "sceneobjecttablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include <algorithm>

SceneObjectTablePanel::SceneObjectTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY);

    table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
                &SceneObjectTablePanel::OnContextMenu, this);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

void SceneObjectTablePanel::InitializeTable()
{
    columnLabels = {"Name", "Layer", "Model File",
                    "Pos X", "Pos Y", "Pos Z",
                    "Rot X", "Rot Y", "Rot Z"};
    std::vector<int> widths = {150, 100, 180,
                               80, 80, 80,
                               80, 80, 80};
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
        wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
        wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
        wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);

        auto euler = MatrixUtils::MatrixToEuler(obj.transform);
        wxString rotX = wxString::Format("%.1f\u00B0", euler[0]);
        wxString rotY = wxString::Format("%.1f\u00B0", euler[1]);
        wxString rotZ = wxString::Format("%.1f\u00B0", euler[2]);

        row.push_back(name);
        row.push_back(layer);
        row.push_back(model);
        row.push_back(posX);
        row.push_back(posY);
        row.push_back(posZ);
        row.push_back(rotX);
        row.push_back(rotY);
        row.push_back(rotZ);

        table->AppendItem(row);
    }

    // Let wxDataViewListCtrl manage column headers and sorting
}

void SceneObjectTablePanel::OnContextMenu(wxDataViewEvent& event)
{
    wxDataViewItem item = event.GetItem();
    int col = event.GetColumn();
    if (!item.IsOk() || col < 0)
        return;

    int row = table->ItemToRow(item);
    if (row == wxNOT_FOUND)
        return;

    wxVariant current;
    table->GetValue(current, row, col);

    wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col], current.GetString());
    if (dlg.ShowModal() == wxID_OK)
    {
        wxString value = dlg.GetValue();
        table->SetValue(wxVariant(value), row, col);
    }
}
