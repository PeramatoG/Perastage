#include "trusstablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include <algorithm>

TrussTablePanel::TrussTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY);

    InitializeTable();
    ReloadData();

    table->Bind(wxEVT_DATAVIEW_COLUMN_HEADER_CLICK,
                &TrussTablePanel::OnColumnHeaderClick, this);

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

void TrussTablePanel::InitializeTable()
{
    columnLabels = {"Name", "Layer", "Model File", "Position", "Rotation"};
    std::vector<int> widths = {150, 100, 180, 150, 150};
    for (size_t i = 0; i < columnLabels.size(); ++i)
        table->AppendTextColumn(columnLabels[i], wxDATAVIEW_CELL_INERT, widths[i]);
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
        wxString pos = wxString::Format("%.1f, %.1f, %.1f", posArr[0], posArr[1], posArr[2]);

        auto euler = MatrixUtils::MatrixToEuler(truss.transform);
        wxString rot = wxString::Format("%.1f\u00B0, %.1f\u00B0, %.1f\u00B0", euler[0], euler[1], euler[2]);

        row.push_back(name);
        row.push_back(layer);
        row.push_back(model);
        row.push_back(pos);
        row.push_back(rot);

        table->AppendItem(row);
    }

    UpdateColumnHeaders();
}

void TrussTablePanel::OnColumnHeaderClick(wxDataViewEvent& event)
{
    int col = event.GetColumn();
    wxMilliClock_t now = wxGetLocalTimeMillis();

    if (lastClickColumn == col && wxMilliClockToLong(now - lastClickTime) < 500)
    {
        lastClickColumn = -1;
        if (sortColumn != col)
        {
            sortColumn = col;
            sortState = 1;
        }
        else
        {
            if (sortState == 1) sortState = -1;
            else if (sortState == -1) { sortState = 0; sortColumn = -1; }
            else sortState = 1;
        }
        ApplySort();
    }
    else
    {
        lastClickColumn = col;
        lastClickTime = now;
    }
}

void TrussTablePanel::ApplySort()
{
    if (sortState == 0 || sortColumn < 0)
    {
        ReloadData();
        return;
    }

    std::vector<std::vector<wxVariant>> rows;
    int rowCount = table->GetItemCount();
    int colCount = table->GetColumnCount();
    for (int r = 0; r < rowCount; ++r)
    {
        std::vector<wxVariant> vals(colCount);
        for (int c = 0; c < colCount; ++c)
        {
            table->GetValue(vals[c], r, c);
        }
        rows.push_back(std::move(vals));
    }

    std::sort(rows.begin(), rows.end(), [this](const auto& a, const auto& b) {
        wxString sa = a[sortColumn].GetString();
        wxString sb = b[sortColumn].GetString();
        int cmp = sa.CmpNoCase(sb);
        return sortState == 1 ? cmp < 0 : cmp > 0;
    });

    table->DeleteAllItems();
    for (const auto& row : rows)
    {
        wxVector<wxVariant> v(row.begin(), row.end());
        table->AppendItem(v);
    }

    UpdateColumnHeaders();
}

void TrussTablePanel::UpdateColumnHeaders()
{
    for (unsigned int i = 0; i < table->GetColumnCount(); ++i)
    {
        wxDataViewColumn* col = table->GetColumn(i);
        wxString label = columnLabels[i];
        if ((int)i == sortColumn && sortState != 0)
            label += sortState == 1 ? wxT(" \u2191") : wxT(" \u2193");
        col->SetTitle(label);
    }
    table->Refresh();
}
