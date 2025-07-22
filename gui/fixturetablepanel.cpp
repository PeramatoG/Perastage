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

    table->Bind(wxEVT_DATAVIEW_COLUMN_HEADER_CLICK,
                &FixtureTablePanel::OnColumnHeaderClick, this);

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
        table->AppendTextColumn(columnLabels[i], wxDATAVIEW_CELL_INERT, widths[i]);
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

    UpdateColumnHeaders();
}

void FixtureTablePanel::OnColumnHeaderClick(wxDataViewEvent& event)
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

void FixtureTablePanel::ApplySort()
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

void FixtureTablePanel::UpdateColumnHeaders()
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

