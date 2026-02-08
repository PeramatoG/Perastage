/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "sceneobjecttablepanel.h"
#include "columnutils.h"
#include "colorfulrenderers.h"
#include "configmanager.h"
#include "layerpanel.h"
#include "matrixutils.h"
#include "stringutils.h"
#include "summarypanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <cctype>
#include <wx/notebook.h>
#include <wx/choicdlg.h>
#include <wx/wupdlock.h> // freeze/thaw UI during batch edits
#include <wx/version.h>

static SceneObjectTablePanel* s_instance = nullptr;

namespace {
struct RangeParts {
    wxArrayString parts;
    bool usedSeparator = false;
    bool trailingSeparator = false;
};

bool IsNumChar(char c)
{
    return std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
           c == '+';
}

RangeParts SplitRangeParts(const wxString& value)
{
    std::string lower = value.Lower().ToStdString();
    std::string normalized;
    normalized.reserve(lower.size() + 4);
    bool usedSeparator = false;
    bool trailingSeparator = false;
    for (size_t i = 0; i < lower.size();)
    {
        if (lower.compare(i, 4, "thru") == 0)
        {
            normalized.push_back(' ');
            usedSeparator = true;
            trailingSeparator = true;
            i += 4;
            continue;
        }
        if (lower[i] == 't')
        {
            char prev = (i > 0) ? lower[i - 1] : '\0';
            char next = (i + 1 < lower.size()) ? lower[i + 1] : '\0';
            bool standalone =
                (i == 0 || std::isspace(static_cast<unsigned char>(prev))) &&
                (i + 1 >= lower.size() ||
                 std::isspace(static_cast<unsigned char>(next)));
            if (standalone || IsNumChar(prev) || IsNumChar(next))
            {
                normalized.push_back(' ');
                usedSeparator = true;
                trailingSeparator = true;
                i += 1;
                continue;
            }
        }
        normalized.push_back(lower[i]);
        if (!std::isspace(static_cast<unsigned char>(lower[i])))
            trailingSeparator = false;
        i += 1;
    }
    wxArrayString rawParts = wxSplit(wxString(normalized), ' ');
    wxArrayString parts;
    for (const auto& part : rawParts)
        if (!part.IsEmpty())
            parts.push_back(part);
    return {parts, usedSeparator, trailingSeparator};
}
} // namespace

SceneObjectTablePanel::SceneObjectTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    store = new ColorfulDataViewListStore();
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, wxDV_MULTIPLE | wxDV_ROW_LINES);
  table->AssociateModel(store);
  store->DecRef();

  table->SetAlternateRowColour(wxColour(40, 40, 40));
  const wxColour selectionBackground(0, 255, 255);
  const wxColour selectionForeground(0, 0, 0);
  store->SetSelectionColours(selectionBackground, selectionForeground);
    table->Bind(wxEVT_LEFT_DOWN, &SceneObjectTablePanel::OnLeftDown, this);
    table->Bind(wxEVT_LEFT_UP, &SceneObjectTablePanel::OnLeftUp, this);
    table->Bind(wxEVT_MOTION, &SceneObjectTablePanel::OnMouseMove, this);
    table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
                &SceneObjectTablePanel::OnSelectionChanged, this);

    table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
                &SceneObjectTablePanel::OnContextMenu, this);
    table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED,
                &SceneObjectTablePanel::OnColumnSorted, this);

    Bind(wxEVT_MOUSE_CAPTURE_LOST, &SceneObjectTablePanel::OnCaptureLost, this);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

SceneObjectTablePanel::~SceneObjectTablePanel()
{
    store = nullptr;
}

void SceneObjectTablePanel::InitializeTable()
{
    columnLabels = {"Name", "Layer", "Model File",
                    "Pos X", "Pos Y", "Pos Z",
                    "Roll (X)", "Pitch (Y)", "Yaw (Z)"};
    std::vector<int> widths = {150, 100, 180,
                               80, 80, 80,
                               80, 80, 80};
    for (size_t i = 0; i < columnLabels.size(); ++i)
        table->AppendColumn(new wxDataViewColumn(
            columnLabels[i], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                      wxALIGN_LEFT),
            i, widths[i], wxALIGN_LEFT,
            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
    ColumnUtils::EnforceMinColumnWidth(table);
}

void SceneObjectTablePanel::ReloadData()
{
    table->DeleteAllItems();
    rowUuids.clear();
    const auto& objs = ConfigManager::Get().GetScene().sceneObjects;

    // Copy objects into a sortable vector
    std::vector<std::pair<std::string, SceneObject>> sortedObjs(objs.begin(), objs.end());

    // Sort by layer and then by name using natural sort for numeric suffixes
    std::sort(sortedObjs.begin(), sortedObjs.end(),
        [](const auto &a, const auto &b) {
            if (a.second.layer == b.second.layer)
                return StringUtils::NaturalLess(a.second.name, b.second.name);
            return StringUtils::NaturalLess(a.second.layer, b.second.layer);
        });

    for (const auto& [uuid, obj] : sortedObjs)
    {
        wxVector<wxVariant> row;

        wxString name = wxString::FromUTF8(obj.name);
        wxString layer = obj.layer == DEFAULT_LAYER_NAME ? wxString()
                                                          : wxString::FromUTF8(obj.layer);
        wxString model = wxString::FromUTF8(obj.modelFile);

        auto posArr = obj.transform.o;
        wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
        wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
        wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);

        auto euler = MatrixUtils::MatrixToEuler(obj.transform);
        wxString roll = wxString::Format("%.1f\u00B0", euler[2]);
        wxString pitch = wxString::Format("%.1f\u00B0", euler[1]);
        wxString yaw = wxString::Format("%.1f\u00B0", euler[0]);

        row.push_back(name);
        row.push_back(layer);
        row.push_back(model);
        row.push_back(posX);
        row.push_back(posY);
        row.push_back(posZ);
        row.push_back(roll);
        row.push_back(pitch);
        row.push_back(yaw);

        store->AppendItem(row, rowUuids.size());
        rowUuids.push_back(uuid);
    }

    // Let wxDataViewListCtrl manage column headers and sorting
    if (LayerPanel::Instance())
        LayerPanel::Instance()->ReloadLayers();
    if (SummaryPanel::Instance() && IsActivePage())
        SummaryPanel::Instance()->ShowSceneObjectSummary();
}

void SceneObjectTablePanel::OnContextMenu(wxDataViewEvent& event)
{
    wxDataViewItem item = event.GetItem();
    int col = event.GetColumn();
    if (!item.IsOk() || col < 0)
        return;

    // Freeze UI updates while performing bulk table modifications. Without
    // freezing, the control repaints after each SetValue call or resort,
    // causing noticeable lag when updating multiple rows. The locker
    // automatically unfreezes the table when it goes out of scope.
    wxWindowUpdateLocker locker(table);

    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        selections.push_back(item);

    // Preserve selection and current row order before edits
    std::vector<std::string> selectedUuids;
    for (const auto& it : selections)
    {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
            selectedUuids.push_back(rowUuids[r]);
    }
    std::vector<std::string> oldOrder = rowUuids;

    int row = table->ItemToRow(item);
    if (row == wxNOT_FOUND)
        return;

    wxVariant current;
    table->GetValue(current, row, col);
    
    if (col == 1)
    {
        auto layers = ConfigManager::Get().GetLayerNames();
        wxArrayString choices;
        for (const auto& n : layers)
            choices.push_back(wxString::FromUTF8(n));
        wxSingleChoiceDialog sdlg(this, "Select layer", "Layer", choices);
        if (sdlg.ShowModal() != wxID_OK)
            return;
        wxString sel = sdlg.GetStringSelection();
        wxString val = sel == wxString::FromUTF8(DEFAULT_LAYER_NAME) ? wxString() : sel;
        for (const auto& itSel : selections)
        {
            int r = table->ItemToRow(itSel);
            if (r != wxNOT_FOUND)
                table->SetValue(wxVariant(val), r, col);
        }
        ResyncRows(oldOrder, selectedUuids);
        UpdateSceneData();
        if (Viewer3DPanel::Instance())
        {
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
        }
        else if (Viewer2DPanel::Instance())
        {
            Viewer2DPanel::Instance()->UpdateScene();
        }
        return;
    }

    wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col], current.GetString());
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString value = dlg.GetValue().Trim(true).Trim(false);

    bool numericCol = (col >= 3);
    bool relative = false;
    double delta = 0.0;
    if (numericCol && col <= 8 && (value.StartsWith("++") || value.StartsWith("--")))
    {
        wxString numStr = value.Mid(2);
        if (numStr.ToDouble(&delta))
        {
            if (value.StartsWith("--"))
                delta = -delta;
            relative = true;
        }
    }

    if (numericCol)
    {
        if (relative)
        {
            for (const auto& it : selections)
            {
                int r = table->ItemToRow(it);
                if (r == wxNOT_FOUND)
                    continue;
                wxVariant cv;
                table->GetValue(cv, r, col);
                wxString cur = cv.GetString();
                if (col >= 6)
                    cur.Replace("\u00B0", "");
                double curVal = 0.0;
                cur.ToDouble(&curVal);
                double newVal = curVal + delta;
                wxString out;
                if (col >= 6)
                    out = wxString::Format("%.1f\u00B0", newVal);
                else
                    out = wxString::Format("%.3f", newVal);
                table->SetValue(wxVariant(out), r, col);
            }
        }
        else
        {
            RangeParts range = SplitRangeParts(value);
            wxArrayString parts = range.parts;
            if (parts.size() == 0 || parts.size() > 2)
            {
                wxMessageBox("Invalid numeric value", "Error", wxOK | wxICON_ERROR);
                return;
            }
            if (range.usedSeparator && parts.size() != 2 &&
                !(parts.size() == 1 && range.trailingSeparator))
            {
                wxMessageBox("Invalid numeric value", "Error", wxOK | wxICON_ERROR);
                return;
            }

            double v1, v2 = 0.0;
            if (!parts[0].ToDouble(&v1))
            {
                wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
                return;
            }
            bool interp = false;
            bool sequential = false;
            if (parts.size() == 2)
            {
                if (!parts[1].ToDouble(&v2))
                {
                    wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
                    return;
                }
                interp = selections.size() > 1;
            }
            else if (range.usedSeparator && range.trailingSeparator)
            {
                sequential = selections.size() > 1;
            }

            for (size_t i = 0; i < selections.size(); ++i)
            {
                double val = v1;
                if (interp)
                    val = v1 + (v2 - v1) * i / (selections.size() - 1);
                else if (sequential)
                    val = v1 + static_cast<double>(i);

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

    // Rebuild row->uuid mapping after potential resort
    ResyncRows(oldOrder, selectedUuids);

    UpdateSceneData();
    if (Viewer3DPanel::Instance())
    {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
    else if (Viewer2DPanel::Instance())
    {
        Viewer2DPanel::Instance()->UpdateScene();
    }
}

void SceneObjectTablePanel::OnLeftDown(wxMouseEvent& evt)
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

void SceneObjectTablePanel::OnLeftUp(wxMouseEvent& evt)
{
    if (dragSelecting)
    {
        dragSelecting = false;
        ReleaseMouse();
    }
    evt.Skip();
}

void SceneObjectTablePanel::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(evt))
{
    dragSelecting = false;
}

void SceneObjectTablePanel::OnMouseMove(wxMouseEvent& evt)
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

void SceneObjectTablePanel::OnSelectionChanged(wxDataViewEvent& evt)
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
    ConfigManager& cfg = ConfigManager::Get();
    if (uuids != cfg.GetSelectedSceneObjects()) {
        cfg.PushUndoState("scene object selection");
        cfg.SetSelectedSceneObjects(uuids);
    }
    if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->SetSelectedFixtures(uuids);
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->SetSelectedUuids(uuids);
    UpdateSelectionHighlight();
    evt.Skip();
}

void SceneObjectTablePanel::UpdateSelectionHighlight()
{
    size_t rowCount = table->GetItemCount();
    std::vector<bool> selectedRows(rowCount, false);
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    for (const auto& it : selections)
    {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND && static_cast<size_t>(r) < rowCount)
            selectedRows[r] = true;
    }
    store->SetSelectedRows(selectedRows);
}

void SceneObjectTablePanel::UpdatePositionValues(
    const std::vector<std::string>& uuids) {
    if (!table)
        return;

    ConfigManager& cfg = ConfigManager::Get();
    auto& scene = cfg.GetScene();
    wxWindowUpdateLocker locker(table);

    for (const auto& uuid : uuids) {
        auto it = scene.sceneObjects.find(uuid);
        if (it == scene.sceneObjects.end())
            continue;

        auto posArr = it->second.transform.o;
        wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
        wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
        wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);

        auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
        if (pos == rowUuids.end())
            continue;

        int row = static_cast<int>(pos - rowUuids.begin());
        table->SetValue(wxVariant(posX), row, 3);
        table->SetValue(wxVariant(posY), row, 4);
        table->SetValue(wxVariant(posZ), row, 5);
    }
}

void SceneObjectTablePanel::ApplyPositionValueUpdates(
    const std::vector<PositionValueUpdate>& updates) {
    if (!table)
        return;

    wxWindowUpdateLocker locker(table);
    for (const auto& update : updates) {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), update.uuid);
        if (pos == rowUuids.end())
            continue;

        int row = static_cast<int>(pos - rowUuids.begin());
        table->SetValue(wxVariant(wxString::FromUTF8(update.posX)), row, 3);
        table->SetValue(wxVariant(wxString::FromUTF8(update.posY)), row, 4);
        table->SetValue(wxVariant(wxString::FromUTF8(update.posZ)), row, 5);
    }
}

void SceneObjectTablePanel::UpdateSceneData()
{
    ConfigManager& cfg = ConfigManager::Get();
    cfg.PushUndoState("edit scene object");
    auto& scene = cfg.GetScene();
    size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());
    for (size_t i = 0; i < count; ++i)
    {
        auto it = scene.sceneObjects.find(rowUuids[i]);
        if (it == scene.sceneObjects.end())
            continue;

        wxVariant v;
        table->GetValue(v, i, 1);
        std::string layerStr = std::string(v.GetString().mb_str());
        if (layerStr.empty())
            it->second.layer.clear();
        else
            it->second.layer = layerStr;
        double x=0, y=0, z=0;
        table->GetValue(v, i, 3); v.GetString().ToDouble(&x);
        table->GetValue(v, i, 4); v.GetString().ToDouble(&y);
        table->GetValue(v, i, 5); v.GetString().ToDouble(&z);

        double roll=0, pitch=0, yaw=0;
        table->GetValue(v, i, 6); {
            wxString s = v.GetString(); s.Replace("\u00B0", ""); s.ToDouble(&roll);
        }
        table->GetValue(v, i, 7); {
            wxString s = v.GetString(); s.Replace("\u00B0", ""); s.ToDouble(&pitch);
        }
        table->GetValue(v, i, 8); {
            wxString s = v.GetString(); s.Replace("\u00B0", ""); s.ToDouble(&yaw);
        }

        Matrix rot = MatrixUtils::EulerToMatrix(static_cast<float>(yaw),
                                                static_cast<float>(pitch),
                                                static_cast<float>(roll));
        it->second.transform = MatrixUtils::ApplyRotationPreservingScale(
            it->second.transform, rot,
            {static_cast<float>(x * 1000.0),
             static_cast<float>(y * 1000.0),
             static_cast<float>(z * 1000.0)});
    }

    if (SummaryPanel::Instance() && IsActivePage())
        SummaryPanel::Instance()->ShowSceneObjectSummary();
}

SceneObjectTablePanel* SceneObjectTablePanel::Instance()
{
    return s_instance;
}

void SceneObjectTablePanel::SetInstance(SceneObjectTablePanel* panel)
{
    s_instance = panel;
}

bool SceneObjectTablePanel::IsActivePage() const
{
    auto* nb = dynamic_cast<wxNotebook*>(GetParent());
    return nb && nb->GetPage(nb->GetSelection()) == this;
}

void SceneObjectTablePanel::HighlightObject(const std::string& uuid)
{
    size_t count =
        std::min(rowUuids.size(), static_cast<size_t>(table->GetItemCount()));
    for (size_t i = 0; i < count; ++i)
    {
        if (!uuid.empty() && rowUuids[i] == uuid)
            store->SetRowBackgroundColour(i, wxColour(0, 200, 0));
        else
            store->ClearRowBackground(i);
    }
    table->Refresh();
}

void SceneObjectTablePanel::ClearSelection() {
    table->UnselectAll();
    UpdateSelectionHighlight();
}

std::vector<std::string> SceneObjectTablePanel::GetSelectedUuids() const {
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    std::vector<std::string> uuids;
    uuids.reserve(selections.size());
    for (const auto& it : selections) {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
            uuids.push_back(rowUuids[r]);
    }
    return uuids;
}

void SceneObjectTablePanel::SelectByUuid(const std::vector<std::string>& uuids) {
    table->UnselectAll();
    std::vector<bool> selectedRows(table->GetItemCount(), false);
    for (const auto& u : uuids) {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), u);
        if (pos != rowUuids.end()) {
            int row = static_cast<int>(pos - rowUuids.begin());
            table->SelectRow(row);
            if (row >= 0 && static_cast<size_t>(row) < selectedRows.size())
                selectedRows[row] = true;
        }
    }
    store->SetSelectedRows(selectedRows);
}

void SceneObjectTablePanel::DeleteSelected()
{
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        return;

    ConfigManager& cfg = ConfigManager::Get();
    cfg.PushUndoState("delete scene object");
    cfg.SetSelectedSceneObjects({});

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
            scene.sceneObjects.erase(rowUuids[r]);
            rowUuids.erase(rowUuids.begin() + r);
            table->DeleteItem(r);
        }
    }

    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->SetSelectedFixtures({});
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
    else if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->SetSelectedUuids({});
        Viewer2DPanel::Instance()->UpdateScene();
    }

    if (SummaryPanel::Instance())
        SummaryPanel::Instance()->ShowSceneObjectSummary();

    std::vector<std::string> order = rowUuids;
    ResyncRows(order, {});
}

void SceneObjectTablePanel::ResyncRows(const std::vector<std::string>& oldOrder,
                                       const std::vector<std::string>& selectedUuids)
{
    unsigned int count = table->GetItemCount();
    std::vector<std::string> newOrder(count);
    for (unsigned int i = 0; i < count; ++i)
    {
        wxDataViewItem it = table->RowToItem(i);
        unsigned long idx = store->GetItemData(it);
        if (idx < oldOrder.size())
            newOrder[i] = oldOrder[idx];
        store->SetItemData(it, i);
    }
    rowUuids.swap(newOrder);

    table->UnselectAll();
    for (const auto& uuid : selectedUuids)
    {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
        if (pos != rowUuids.end())
            table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
    }
    UpdateSelectionHighlight();
}

void SceneObjectTablePanel::OnColumnSorted(wxDataViewEvent& event)
{
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    std::vector<std::string> selectedUuids;
    for (const auto& it : selections)
    {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
            selectedUuids.push_back(rowUuids[r]);
    }
    std::vector<std::string> oldOrder = rowUuids;
    ResyncRows(oldOrder, selectedUuids);
    event.Skip();
}
