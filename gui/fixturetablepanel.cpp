#include "fixturetablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include <wx/tokenzr.h>
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
        "Universe",
        "Channel",
        "GDTF",
        "Pos X",
        "Pos Y",
        "Pos Z",
        "Hang Pos",
        "Rot X",
        "Rot Y",
        "Rot Z"
    };

    std::vector<int> widths = {150, 90, 100, 80, 80, 180,
                               80, 80, 80, 120,
                               80, 80, 80};
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

    // Column 2: Layer (string)
    table->AppendTextColumn(columnLabels[2], wxDATAVIEW_CELL_INERT, widths[2],
                            wxALIGN_LEFT, flags);

    // Column 3: Universe (numeric)
    auto* uniRenderer =
        new wxDataViewTextRenderer("long", wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
    auto* uniColumn = new wxDataViewColumn(columnLabels[3], uniRenderer, 3,
                                           widths[3], wxALIGN_LEFT, flags);
    table->AppendColumn(uniColumn);

    // Column 4: Channel (numeric)
    auto* chRenderer =
        new wxDataViewTextRenderer("long", wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
    auto* chColumn = new wxDataViewColumn(columnLabels[4], chRenderer, 4,
                                          widths[4], wxALIGN_LEFT, flags);
    table->AppendColumn(chColumn);

    // Remaining columns as regular text
    for (size_t i = 5; i < columnLabels.size(); ++i)
        table->AppendTextColumn(columnLabels[i], wxDATAVIEW_CELL_INERT,
                                widths[i], wxALIGN_LEFT, flags);
}

void FixtureTablePanel::ReloadData()
{
    table->DeleteAllItems();

    const auto& fixtures = ConfigManager::Get().GetScene().fixtures;

    struct Address { long universe; long channel; };
    auto parseAddress = [](const std::string& addr) -> Address {
        Address res{0, 0};
        if (!addr.empty())
        {
            size_t dot = addr.find('.');
            if (dot != std::string::npos)
            {
                try { res.universe = std::stol(addr.substr(0, dot)); } catch (...) {}
                try { res.channel = std::stol(addr.substr(dot + 1)); } catch (...) {}
            }
        }
        return res;
    };

    std::vector<const Fixture*> sorted;
    sorted.reserve(fixtures.size());
    for (const auto& [uuid, fixture] : fixtures)
        sorted.push_back(&fixture);

    std::sort(sorted.begin(), sorted.end(), [&](const Fixture* a, const Fixture* b) {
        if (a->fixtureId != b->fixtureId)
            return a->fixtureId < b->fixtureId;
        if (a->gdtfSpec != b->gdtfSpec)
            return a->gdtfSpec < b->gdtfSpec;
        auto addrA = parseAddress(a->address);
        auto addrB = parseAddress(b->address);
        if (addrA.universe != addrB.universe)
            return addrA.universe < addrB.universe;
        return addrA.channel < addrB.channel;
    });

    for (const Fixture* fixture : sorted)
    {
        wxVector<wxVariant> row;

        wxString name = wxString::FromUTF8(fixture->name);
        long fixtureID = static_cast<long>(fixture->fixtureId);
        wxString layer = wxString::FromUTF8(fixture->layer);
        long universe = 0;
        long channel = 0;
        if (!fixture->address.empty())
        {
            wxStringTokenizer tk(wxString::FromUTF8(fixture->address), ".");
            if (tk.HasMoreTokens()) tk.GetNextToken().ToLong(&universe);
            if (tk.HasMoreTokens()) tk.GetNextToken().ToLong(&channel);
        }
        wxString gdtf = wxString::FromUTF8(fixture->gdtfSpec);

        auto posArr = fixture->GetPosition();
        wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
        wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
        wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);
        wxString posName = wxString::FromUTF8(fixture->positionName);

        auto euler = MatrixUtils::MatrixToEuler(fixture->transform);
        wxString rotX = wxString::Format("%.1f\u00B0", euler[0]);
        wxString rotY = wxString::Format("%.1f\u00B0", euler[1]);
        wxString rotZ = wxString::Format("%.1f\u00B0", euler[2]);

        row.push_back(name);
        row.push_back(fixtureID);
        row.push_back(layer);
        row.push_back(universe);
        row.push_back(channel);
        row.push_back(gdtf);
        row.push_back(posX);
        row.push_back(posY);
        row.push_back(posZ);
        row.push_back(posName);
        row.push_back(rotX);
        row.push_back(rotY);
        row.push_back(rotZ);

        table->AppendItem(row);
    }

    // Let wxDataViewListCtrl manage column headers and sorting
}


