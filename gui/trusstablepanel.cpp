#include "trusstablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include <algorithm>

TrussTablePanel::TrussTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, wxDV_MULTIPLE | wxDV_ROW_LINES);
    table->EnableAlternateRowColours(true);
    table->AssociateModel(&store);
    store.DecRef();

    table->Bind(wxEVT_LEFT_DOWN, &TrussTablePanel::OnLeftDown, this);
    table->Bind(wxEVT_LEFT_UP, &TrussTablePanel::OnLeftUp, this);
    table->Bind(wxEVT_MOTION, &TrussTablePanel::OnMouseMove, this);

    table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
                &TrussTablePanel::OnContextMenu, this);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

void TrussTablePanel::InitializeTable()
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

void TrussTablePanel::ReloadData()
{
    table->DeleteAllItems();
    const auto& trusses = ConfigManager::Get().GetScene().trusses;

    for (const auto& [uuid, truss] : trusses)
    {
        wxVector<wxVariant> row;

        wxString name = wxString::FromUTF8(truss.name);
        wxString layer = wxString::FromUTF8(truss.layer);
        wxString model = wxString::FromUTF8(truss.symbolFile);

        auto posArr = truss.transform.o;
        wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
        wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
        wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);

        auto euler = MatrixUtils::MatrixToEuler(truss.transform);
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

void TrussTablePanel::OnContextMenu(wxDataViewEvent& event)
{
    wxDataViewItem item = event.GetItem();
    int col = event.GetColumn();
    if (!item.IsOk() || col < 0)
        return;

    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        selections.push_back(item);

    int row = table->ItemToRow(item);
    if (row == wxNOT_FOUND)
        return;

    wxVariant current;
    table->GetValue(current, row, col);

    wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col], current.GetString());
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString value = dlg.GetValue().Trim(true).Trim(false);

    bool numericCol = (col >= 3);

    if (numericCol)
    {
        wxArrayString parts = wxSplit(value, ' ');
        if (parts.size() == 0 || parts.size() > 2)
        {
            wxMessageBox("Valor num\xE9rico inv\xE1lido", "Error", wxOK | wxICON_ERROR);
            return;
        }

        double v1, v2 = 0.0;
        if (!parts[0].ToDouble(&v1))
        {
            wxMessageBox("Valor inv\xE1lido", "Error", wxOK | wxICON_ERROR);
            return;
        }
        bool interp = false;
        if (parts.size() == 2)
        {
            if (!parts[1].ToDouble(&v2))
            {
                wxMessageBox("Valor inv\xE1lido", "Error", wxOK | wxICON_ERROR);
                return;
            }
            interp = selections.size() > 1;
        }

        for (size_t i = 0; i < selections.size(); ++i)
        {
            double val = v1;
            if (interp)
                val = v1 + (v2 - v1) * i / (selections.size() - 1);

            wxString out;
            if (col >= 6)
                out = wxString::Format("%.1f\u00B0", val);
            else
                out = wxString::Format("%.3f", val);

            int r = table->ItemToRow(selections[i]);
            if (r != wxNOT_FOUND)
                table->SetValue(wxVariant(out), r, col);
        }
    }
    else
    {
        for (const auto& it : selections)
        {
            int r = table->ItemToRow(it);
            if (r != wxNOT_FOUND)
                table->SetValue(wxVariant(value), r, col);
        }
    }
}

void TrussTablePanel::OnLeftDown(wxMouseEvent& evt)
{
    wxDataViewItem item;
    wxDataViewColumn* col;
    table->HitTest(evt.GetPosition(), item, col);
    startRow = table->ItemToRow(item);
    if (startRow != wxNOT_FOUND)
    {
        dragSelecting = true;
        table->UnselectAll();
        table->SelectRow(startRow);
        CaptureMouse();
    }
    evt.Skip();
}

void TrussTablePanel::OnLeftUp(wxMouseEvent& evt)
{
    if (dragSelecting)
    {
        dragSelecting = false;
        ReleaseMouse();
    }
    evt.Skip();
}

void TrussTablePanel::OnMouseMove(wxMouseEvent& evt)
{
    if (!dragSelecting || !evt.Dragging())
    {
        evt.Skip();
        return;
    }
    wxDataViewItem item;
    wxDataViewColumn* col;
    table->HitTest(evt.GetPosition(), item, col);
    int row = table->ItemToRow(item);
    if (row != wxNOT_FOUND)
    {
        int minRow = std::min(startRow, row);
        int maxRow = std::max(startRow, row);
        table->UnselectAll();
        for (int r = minRow; r <= maxRow; ++r)
            table->SelectRow(r);
    }
    evt.Skip();
}
