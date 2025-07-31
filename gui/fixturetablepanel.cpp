#include "fixturetablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include "viewer3dpanel.h"
#include "gdtfloader.h"
#include <filesystem>
#include <wx/tokenzr.h>
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/choicdlg.h>
#include <algorithm>
#include <wx/settings.h>
#include <wx/notebook.h>

namespace fs = std::filesystem;

FixtureTablePanel::FixtureTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, wxDV_MULTIPLE | wxDV_ROW_LINES);
#if defined(wxHAS_GENERIC_DATAVIEWCTRL)
    table->SetAlternateRowColour(
        wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
#endif
    table->AssociateModel(&store);

    table->Bind(wxEVT_LEFT_DOWN, &FixtureTablePanel::OnLeftDown, this);
    table->Bind(wxEVT_LEFT_UP, &FixtureTablePanel::OnLeftUp, this);
    table->Bind(wxEVT_MOTION, &FixtureTablePanel::OnMouseMove, this);
    table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
                &FixtureTablePanel::OnSelectionChanged, this);

    table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
                &FixtureTablePanel::OnContextMenu, this);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

FixtureTablePanel::~FixtureTablePanel()
{
    if (table)
        table->AssociateModel(nullptr);
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
        "Mode",
        "Ch Count",
        "Pos X",
        "Pos Y",
        "Pos Z",
        "Hang Pos",
        "Rot X",
        "Rot Y",
        "Rot Z"
    };

    std::vector<int> widths = {150, 90, 100, 80, 80, 180, 120, 80,
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
    gdtfPaths.clear();
    rowUuids.clear();

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

    std::vector<std::pair<std::string,const Fixture*>> sorted;
    sorted.reserve(fixtures.size());
    for (const auto& [uuid, fixture] : fixtures)
        sorted.emplace_back(uuid, &fixture);

    std::sort(sorted.begin(), sorted.end(), [&](const auto& A, const auto& B) {
        const Fixture* a = A.second;
        const Fixture* b = B.second;
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

    for (const auto& pair : sorted)
    {
        const std::string& uuid = pair.first;
        const Fixture* fixture = pair.second;
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
        std::string fullPath;
        if (!fixture->gdtfSpec.empty()) {
            const std::string& base = ConfigManager::Get().GetScene().basePath;
            fs::path p = base.empty() ? fs::path(fixture->gdtfSpec)
                                     : fs::path(base) / fixture->gdtfSpec;
            fullPath = p.string();
        }
        wxString gdtfFull = wxString::FromUTF8(fullPath);
        gdtfPaths.push_back(gdtfFull);
        wxString gdtf = wxFileName(gdtfFull).GetFullName();
        wxString mode = wxString::FromUTF8(fixture->gdtfMode);

        int chCount = GetGdtfModeChannelCount(gdtfFull.ToStdString(),
                                              fixture->gdtfMode);
        wxString chCountStr = chCount >= 0 ? wxString::Format("%d", chCount)
                                           : wxString();

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
        row.push_back(mode);
        row.push_back(chCountStr);
        row.push_back(posX);
        row.push_back(posY);
        row.push_back(posZ);
        row.push_back(posName);
        row.push_back(rotX);
        row.push_back(rotY);
        row.push_back(rotZ);

        table->AppendItem(row);
        rowUuids.push_back(uuid);
    }

    if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->SetSelectedFixtures({});

// Let wxDataViewListCtrl manage column headers and sorting
}

void FixtureTablePanel::OnContextMenu(wxDataViewEvent& event)
{
    wxDataViewItem item = event.GetItem();
    int col = event.GetColumn();
    if (!item.IsOk() || col < 0)
        return;

    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        selections.push_back(item);

    // GDTF column opens file dialog
    if (col == 5)
    {
        wxFileDialog fdlg(this, "Select GDTF file", wxEmptyString, wxEmptyString,
                          "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (fdlg.ShowModal() == wxID_OK)
        {
            wxString path = fdlg.GetPath();
            wxString name = fdlg.GetFilename();
            std::vector<wxString> affectedNames;

            for (const auto& it : selections)
            {
                int r = table->ItemToRow(it);
                if (r == wxNOT_FOUND)
                    continue;

                // Ensure vector size
                if ((size_t)r >= gdtfPaths.size())
                    gdtfPaths.resize(table->GetItemCount());

                // Track name for propagation
                wxVariant nv;
                table->GetValue(nv, r, 0);
                affectedNames.push_back(nv.GetString());

                // Update selected row
                gdtfPaths[r] = path;
                table->SetValue(wxVariant(name), r, col);
            }

            // Apply same GDTF to all fixtures sharing the same name
            for (size_t i = 0; i < table->GetItemCount(); ++i)
            {
                wxVariant nv;
                table->GetValue(nv, i, 0);
                if (std::find(affectedNames.begin(), affectedNames.end(), nv.GetString()) != affectedNames.end())
                {
                    if (i >= gdtfPaths.size())
                        gdtfPaths.resize(table->GetItemCount());
                    gdtfPaths[i] = path;
                    table->SetValue(wxVariant(name), i, col);
                }
            }

            ApplyModeForGdtf(path);
        }
        UpdateSceneData();
        if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
        }
        return;
    }

    // Mode column shows available modes of the selected GDTF
    if (col == 6)
    {
        int r = table->ItemToRow(item);
        if (r == wxNOT_FOUND)
            return;

        wxString gdtfPath;
        if ((size_t)r < gdtfPaths.size())
            gdtfPath = gdtfPaths[r];

        std::vector<std::string> modes =
            GetGdtfModes(gdtfPath.ToStdString());
        if (modes.size() <= 1)
            return;

        wxArrayString choices;
        for (const auto& m : modes)
            choices.push_back(wxString::FromUTF8(m));

        wxSingleChoiceDialog dlg(this, "Select DMX mode", "DMX Mode", choices);
        if (dlg.ShowModal() != wxID_OK)
            return;

        wxString sel = dlg.GetStringSelection();

        wxDataViewItemArray modeSelections;
        table->GetSelections(modeSelections);
        if (modeSelections.empty())
            modeSelections.push_back(item);

        for (const auto& itSel : modeSelections)
        {
            int sr = table->ItemToRow(itSel);
            if (sr == wxNOT_FOUND)
                continue;
            if ((size_t)sr >= gdtfPaths.size())
                continue;
            if (gdtfPaths[sr] != gdtfPath)
                continue;

            table->SetValue(wxVariant(sel), sr, col);

            int chCount = GetGdtfModeChannelCount(gdtfPath.ToStdString(),
                                                  sel.ToStdString());
            wxString chStr = chCount >= 0 ? wxString::Format("%d", chCount)
                                          : wxString();
            table->SetValue(wxVariant(chStr), sr, 7);
        }

        UpdateSceneData();
        if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
        }
        return;
    }

    int baseRow = table->ItemToRow(item);
    if (baseRow == wxNOT_FOUND)
        return;

    wxVariant current;
    table->GetValue(current, baseRow, col);

    wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col], current.GetString());
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString value = dlg.GetValue().Trim(true).Trim(false);

    bool intCol = (col == 1 || col == 3 || col == 4);
    bool numericCol = intCol || (col >= 8 && col <= 10) || (col >= 12 && col <= 14);

    wxArrayString parts = wxSplit(value, ' ');

    if (numericCol)
    {
        if (parts.size() == 0 || parts.size() > 2)
        {
            wxMessageBox("Valor num\xE9rico inv\xE1lido", "Error", wxOK | wxICON_ERROR);
            return;
        }

        if (intCol)
        {
            long v1, v2 = 0;
            if (!parts[0].ToLong(&v1))
            {
                wxMessageBox("Valor inv\xE1lido", "Error", wxOK | wxICON_ERROR);
                return;
            }
            if (col == 4 && (v1 < 1 || v1 > 512))
            {
                wxMessageBox("Channel fuera de rango (1-512)", "Error", wxOK | wxICON_ERROR);
                return;
            }
            bool interp = false;
            if (parts.size() == 2)
            {
                if (!parts[1].ToLong(&v2))
                {
                    wxMessageBox("Valor inv\xE1lido", "Error", wxOK | wxICON_ERROR);
                    return;
                }
                if (col == 4 && (v2 < 1 || v2 > 512))
                {
                    wxMessageBox("Channel fuera de rango (1-512)", "Error", wxOK | wxICON_ERROR);
                    return;
                }
                interp = selections.size() > 1;
            }

            for (size_t i = 0; i < selections.size(); ++i)
            {
                long val = v1;
                if (interp)
                    val = static_cast<long>(v1 + (double)(v2 - v1) * i / (selections.size() - 1));

                int r = table->ItemToRow(selections[i]);
                if (r != wxNOT_FOUND)
                    table->SetValue(wxVariant(val), r, col);
            }
        }
        else // floating point stored as string
        {
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
                if (col >= 12 && col <= 14)
                    out = wxString::Format("%.1f\u00B0", val);
                else
                    out = wxString::Format("%.3f", val);

                int r = table->ItemToRow(selections[i]);
                if (r != wxNOT_FOUND)
                    table->SetValue(wxVariant(out), r, col);
            }
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

    UpdateSceneData();
    if (Viewer3DPanel::Instance())
    {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
}

static FixtureTablePanel* s_instance = nullptr;

FixtureTablePanel* FixtureTablePanel::Instance()
{
    return s_instance;
}

void FixtureTablePanel::SetInstance(FixtureTablePanel* panel)
{
    s_instance = panel;
}

bool FixtureTablePanel::IsActivePage() const
{
    auto* nb = dynamic_cast<wxNotebook*>(GetParent());
    return nb && nb->GetPage(nb->GetSelection()) == this;
}

void FixtureTablePanel::HighlightFixture(const std::string& uuid)
{
    for (size_t i = 0; i < rowUuids.size(); ++i)
    {
        if (!uuid.empty() && rowUuids[i] == uuid)
            store.SetRowBackgroundColour(i, wxColour(0, 200, 0));
        else
            store.ClearRowBackground(i);
    }
    table->Refresh();
}

void FixtureTablePanel::ClearSelection()
{
    table->UnselectAll();
}

void FixtureTablePanel::DeleteSelected()
{
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        return;

    ConfigManager& cfg = ConfigManager::Get();
    cfg.PushUndoState();

    std::vector<int> rows;
    rows.reserve(selections.size());
    for (const auto& it : selections) {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND)
            rows.push_back(r);
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    auto& scene = ConfigManager::Get().GetScene();
    for (int r : rows) {
        if ((size_t)r < rowUuids.size()) {
            scene.fixtures.erase(rowUuids[r]);
            rowUuids.erase(rowUuids.begin() + r);
            if ((size_t)r < gdtfPaths.size())
                gdtfPaths.erase(gdtfPaths.begin() + r);
            table->DeleteItem(r);
        }
    }

    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
}

void FixtureTablePanel::OnLeftDown(wxMouseEvent& evt)
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

void FixtureTablePanel::OnLeftUp(wxMouseEvent& evt)
{
    if (dragSelecting)
    {
        dragSelecting = false;
        ReleaseMouse();
    }
    evt.Skip();
}

void FixtureTablePanel::OnMouseMove(wxMouseEvent& evt)
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

void FixtureTablePanel::OnSelectionChanged(wxDataViewEvent& evt)
{
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    std::vector<std::string> uuids;
    uuids.reserve(selections.size());
    for (const auto& it : selections)
    {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
            uuids.push_back(rowUuids[r]);
    }
    if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->SetSelectedFixtures(uuids);
    evt.Skip();
}

void FixtureTablePanel::UpdateSceneData()
{
    ConfigManager& cfg = ConfigManager::Get();
    cfg.PushUndoState();
    auto& scene = cfg.GetScene();
    size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());
    for (size_t i = 0; i < count; ++i)
    {
        auto it = scene.fixtures.find(rowUuids[i]);
        if (it == scene.fixtures.end())
            continue;

        if (i < gdtfPaths.size())
            it->second.gdtfSpec = std::string(gdtfPaths[i].mb_str());

        wxVariant v;
        table->GetValue(v, i, 1);
        long fid = v.GetLong();
        it->second.fixtureId = static_cast<int>(fid);

        table->GetValue(v, i, 3);
        long uni = v.GetLong();

        table->GetValue(v, i, 4);
        long ch = v.GetLong();

        table->GetValue(v, i, 6);
        it->second.gdtfMode = std::string(v.GetString().mb_str());

        if (uni>0 && ch>0)
            it->second.address = wxString::Format("%ld.%ld", uni, ch).ToStdString();
        else
            it->second.address.clear();

        double x=0, y=0, z=0;
        table->GetValue(v, i, 8); v.GetString().ToDouble(&x);
        table->GetValue(v, i, 9); v.GetString().ToDouble(&y);
        table->GetValue(v, i, 10); v.GetString().ToDouble(&z);
        it->second.transform.o = {static_cast<float>(x * 1000.0),
                                  static_cast<float>(y * 1000.0),
                                  static_cast<float>(z * 1000.0)};
    }
}

void FixtureTablePanel::ApplyModeForGdtf(const wxString& path)
{
    if (path.empty())
        return;

    std::vector<std::string> modes = GetGdtfModes(path.ToStdString());
    if (modes.empty())
        return;

    auto toLower = [](const std::string& s) {
        std::string out(s);
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return out;
    };

    for (size_t i = 0; i < gdtfPaths.size() && i < (size_t)table->GetItemCount(); ++i)
    {
        if (gdtfPaths[i] != path)
            continue;

        wxVariant v;
        table->GetValue(v, i, 6);
        wxString currWx = v.GetString();
        std::string curr = std::string(currWx.mb_str());

        std::string chosen = curr;
        bool found = std::find(modes.begin(), modes.end(), curr) != modes.end();
        if (!found)
        {
            for (const std::string& m : modes)
            {
                std::string low = toLower(m);
                if (low == "default" || low == "standard")
                {
                    chosen = m;
                    found = true;
                    break;
                }
            }
            if (!found)
                chosen = modes.front();
        }

        if (chosen != curr)
            table->SetValue(wxVariant(wxString::FromUTF8(chosen)), i, 6);

        int chCount = GetGdtfModeChannelCount(path.ToStdString(), chosen);
        wxString chStr = chCount >= 0 ? wxString::Format("%d", chCount)
                                      : wxString();
        table->SetValue(wxVariant(chStr), i, 7);
    }
}


