#include "sceneobjecttablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <wx/settings.h>
#include <wx/notebook.h>

static SceneObjectTablePanel* s_instance = nullptr;

SceneObjectTablePanel::SceneObjectTablePanel(wxWindow* parent)
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

    table->Bind(wxEVT_LEFT_DOWN, &SceneObjectTablePanel::OnLeftDown, this);
    table->Bind(wxEVT_LEFT_UP, &SceneObjectTablePanel::OnLeftUp, this);
    table->Bind(wxEVT_MOTION, &SceneObjectTablePanel::OnMouseMove, this);
    table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
                &SceneObjectTablePanel::OnSelectionChanged, this);

    table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
                &SceneObjectTablePanel::OnContextMenu, this);
    table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED,
                &SceneObjectTablePanel::OnColumnSorted, this);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

SceneObjectTablePanel::~SceneObjectTablePanel()
{
    if (table)
        table->AssociateModel(nullptr);
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
    rowUuids.clear();
    const auto& objs = ConfigManager::Get().GetScene().sceneObjects;

    // Copy objects into a sortable vector
    std::vector<std::pair<std::string, SceneObject>> sortedObjs(objs.begin(), objs.end());

    // Sort by layer and then by name
    std::sort(sortedObjs.begin(), sortedObjs.end(),
        [](const auto& a, const auto& b) {
            if (a.second.layer == b.second.layer)
                return a.second.name < b.second.name;
            return a.second.layer < b.second.layer;
        });

    for (const auto& [uuid, obj] : sortedObjs)
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

        store.AppendItem(row, rowUuids.size());
        rowUuids.push_back(uuid);
    }

    // Let wxDataViewListCtrl manage column headers and sorting
}

void SceneObjectTablePanel::OnContextMenu(wxDataViewEvent& event)
{
    wxDataViewItem item = event.GetItem();
    int col = event.GetColumn();
    if (!item.IsOk() || col < 0)
        return;

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
        if (parts.size() == 2)
        {
            if (!parts[1].ToDouble(&v2))
            {
                wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
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

    // Rebuild row->uuid mapping after potential resort
    ResyncRows(oldOrder, selectedUuids);

    UpdateSceneData();
    if (Viewer3DPanel::Instance())
    {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
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
    evt.Skip();
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
        double x=0, y=0, z=0;
        table->GetValue(v, i, 3); v.GetString().ToDouble(&x);
        table->GetValue(v, i, 4); v.GetString().ToDouble(&y);
        table->GetValue(v, i, 5); v.GetString().ToDouble(&z);
        it->second.transform.o = {static_cast<float>(x * 1000.0),
                                  static_cast<float>(y * 1000.0),
                                  static_cast<float>(z * 1000.0)};
    }
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
    for (size_t i = 0; i < rowUuids.size(); ++i)
    {
        if (!uuid.empty() && rowUuids[i] == uuid)
            store.SetRowBackgroundColour(i, wxColour(0, 200, 0));
        else
            store.ClearRowBackground(i);
    }
    table->Refresh();
}

void SceneObjectTablePanel::ClearSelection() {
    table->UnselectAll();
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
    for (const auto& u : uuids) {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), u);
        if (pos != rowUuids.end())
            table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
    }
}

void SceneObjectTablePanel::DeleteSelected()
{
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        return;

    ConfigManager& cfg = ConfigManager::Get();
    cfg.PushUndoState("delete scene object");

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
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }

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
        unsigned long idx = store.GetItemData(it);
        if (idx < oldOrder.size())
            newOrder[i] = oldOrder[idx];
        store.SetItemData(it, i);
    }
    rowUuids.swap(newOrder);

    table->UnselectAll();
    for (const auto& uuid : selectedUuids)
    {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
        if (pos != rowUuids.end())
            table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
    }
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
