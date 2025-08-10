#include "trusstablepanel.h"
#include "configmanager.h"
#include "matrixutils.h"
#include "viewer3dpanel.h"
#include "layerpanel.h"
#include "stringutils.h"
#include "projectutils.h"
#include "trussdictionary.h"
#include "trussloader.h"
#include <filesystem>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <algorithm>
#include <unordered_map>
#include <wx/notebook.h>
#include <wx/choicdlg.h>

static TrussTablePanel* s_instance = nullptr;
namespace fs = std::filesystem;

TrussTablePanel::TrussTablePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, wxDV_MULTIPLE | wxDV_ROW_LINES);
    table->AssociateModel(&store);

    table->SetAlternateRowColour(wxColour(40, 40, 40));

    table->Bind(wxEVT_LEFT_DOWN, &TrussTablePanel::OnLeftDown, this);
    table->Bind(wxEVT_LEFT_UP, &TrussTablePanel::OnLeftUp, this);
    table->Bind(wxEVT_MOTION, &TrussTablePanel::OnMouseMove, this);
    table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
                &TrussTablePanel::OnSelectionChanged, this);

    table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
                &TrussTablePanel::OnContextMenu, this);
    table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED,
                &TrussTablePanel::OnColumnSorted, this);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

TrussTablePanel::~TrussTablePanel()
{
    if (table)
        table->AssociateModel(nullptr);
}

void TrussTablePanel::InitializeTable()
{
    columnLabels = {"Name", "Layer", "Model File", "Hang Pos",
                    "Pos X", "Pos Y", "Pos Z",
                    "Rot X", "Rot Y", "Rot Z",
                    "Manufacturer", "Model",
                    "Length (m)", "Weight (kg)"};
    std::vector<int> widths = {150, 100, 180, 120,
                               80, 80, 80,
                               80, 80, 80,
                               120, 120,
                               90, 90};
    for (size_t i = 0; i < columnLabels.size(); ++i)
        table->AppendTextColumn(
            columnLabels[i], wxDATAVIEW_CELL_INERT, widths[i], wxALIGN_LEFT,
            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
}

void TrussTablePanel::ReloadData()
{
    table->DeleteAllItems();
    rowUuids.clear();
    modelPaths.clear();
    const auto& trusses = ConfigManager::Get().GetScene().trusses;

    std::vector<std::pair<std::string, const Truss*>> sorted;
    sorted.reserve(trusses.size());
    for (const auto& [uuid, truss] : trusses)
        sorted.emplace_back(uuid, &truss);

    std::sort(sorted.begin(), sorted.end(), [](const auto &A, const auto &B) {
      const Truss *a = A.second;
      const Truss *b = B.second;
      if (a->layer != b->layer)
        return StringUtils::NaturalLess(a->layer, b->layer);
      if (a->positionName != b->positionName)
        return StringUtils::NaturalLess(a->positionName, b->positionName);
      return StringUtils::NaturalLess(a->name, b->name);
    });

    for (const auto& pair : sorted)
    {
        const std::string& uuid = pair.first;
        const Truss& truss = *pair.second;
        wxVector<wxVariant> row;

        wxString name = wxString::FromUTF8(truss.name);
        wxString layer = truss.layer == DEFAULT_LAYER_NAME ? wxString()
                                                            : wxString::FromUTF8(truss.layer);
        std::string fullPath;
        if (!truss.symbolFile.empty()) {
            const std::string &base = ConfigManager::Get().GetScene().basePath;
            fs::path p = base.empty() ? fs::path(truss.symbolFile)
                                     : fs::path(base) / truss.symbolFile;
            fullPath = p.string();
        }
        wxString modelFull = wxString::FromUTF8(fullPath);
        modelPaths.push_back(modelFull);
        wxString model = wxFileName(modelFull).GetFullName();

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
        wxString posName = wxString::FromUTF8(truss.positionName);
        row.push_back(posName);
        row.push_back(posX);
        row.push_back(posY);
        row.push_back(posZ);
        row.push_back(rotX);
        row.push_back(rotY);
        row.push_back(rotZ);
        wxString manuf = wxString::FromUTF8(truss.manufacturer);
        wxString modelName = wxString::FromUTF8(truss.model);
        wxString len = wxString::Format("%.2f", truss.lengthMm / 1000.0f);
        wxString weight = wxString::Format("%.2f", truss.weightKg);
        row.push_back(manuf);
        row.push_back(modelName);
        row.push_back(len);
        row.push_back(weight);

        store.AppendItem(row, rowUuids.size());
        rowUuids.push_back(uuid);
    }

    // Let wxDataViewListCtrl manage column headers and sorting
    if (LayerPanel::Instance())
        LayerPanel::Instance()->ReloadLayers();
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

    // Layer column uses a dropdown of existing layers
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
        return;
    }

    // Model File column opens file dialog
    if (col == 2)
    {
        wxString trussDir =
            wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses"));
        wxFileDialog fdlg(this, "Select Truss Model", trussDir, wxEmptyString,
                          "Truss files (*.gtruss;*.3ds;*.glb)|*.gtruss;*.3ds;*.glb|All files|*.*",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (fdlg.ShowModal() == wxID_OK)
        {
            wxString selPath = fdlg.GetPath();
            std::string pathUtf8(selPath.ToUTF8());
            Truss parsed;
            bool parsedOk = false;
            wxString manuf, modelNameWx, lenStr, weightStr;
            std::string modelKey;
            if (fs::path(pathUtf8).extension() == ".gtruss" &&
                LoadTrussArchive(pathUtf8, parsed))
            {
                pathUtf8 = parsed.symbolFile;
                manuf = wxString::FromUTF8(parsed.manufacturer);
                modelNameWx = wxString::FromUTF8(parsed.model);
                lenStr = wxString::Format("%.2f", parsed.lengthMm / 1000.0f);
                weightStr = wxString::Format("%.2f", parsed.weightKg);
                modelKey = parsed.model;
                parsedOk = true;
            }
            if (!parsedOk)
            {
                wxVariant mv;
                table->GetValue(mv, row, 11);
                modelNameWx = mv.GetString();
                modelKey = std::string(modelNameWx.ToUTF8());
            }
            wxString fileName =
                wxFileName(wxString::FromUTF8(pathUtf8)).GetFullName();
            if (modelPaths.size() < table->GetItemCount())
                modelPaths.resize(table->GetItemCount());
            for (const auto& itSel : selections)
            {
                int r = table->ItemToRow(itSel);
                if (r == wxNOT_FOUND)
                    continue;
                modelPaths[r] = wxString::FromUTF8(pathUtf8);
                table->SetValue(wxVariant(fileName), r, 2);
                if (parsedOk)
                {
                    table->SetValue(wxVariant(manuf), r, 10);
                    table->SetValue(wxVariant(modelNameWx), r, 11);
                    table->SetValue(wxVariant(lenStr), r, 12);
                    table->SetValue(wxVariant(weightStr), r, 13);
                }
            }
            for (unsigned int i = 0; i < table->GetItemCount(); ++i)
            {
                wxVariant mv;
                table->GetValue(mv, i, 11);
                if (mv.GetString() == modelNameWx)
                {
                    modelPaths[i] = wxString::FromUTF8(pathUtf8);
                    table->SetValue(wxVariant(fileName), i, 2);
                }
            }
            TrussDictionary::Update(modelKey, pathUtf8);
            ResyncRows(oldOrder, selectedUuids);
            UpdateSceneData();
            if (Viewer3DPanel::Instance())
            {
                Viewer3DPanel::Instance()->UpdateScene();
                Viewer3DPanel::Instance()->Refresh();
            }
        }
        return;
    }

    wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col], current.GetString());
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString value = dlg.GetValue().Trim(true).Trim(false);

    bool numericCol = (col >= 4 && col <= 9);
    bool relative = false;
    double delta = 0.0;
    if (numericCol && !value.empty() && (value[0] == '+' || value[0] == '-'))
    {
        wxString numStr = value.Mid(1);
        if (numStr.ToDouble(&delta))
        {
            if (value[0] == '-')
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
                if (col >= 7)
                    cur.Replace("\u00B0", "");
                double curVal = 0.0;
                cur.ToDouble(&curVal);
                double newVal = curVal + delta;
                wxString out;
                if (col >= 7)
                    out = wxString::Format("%.1f\u00B0", newVal);
                else
                    out = wxString::Format("%.3f", newVal);
                table->SetValue(wxVariant(out), r, col);
            }
        }
        else
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
                if (col >= 7)
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

    ResyncRows(oldOrder, selectedUuids);

    UpdateSceneData();
    if (Viewer3DPanel::Instance())
    {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
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

void TrussTablePanel::OnSelectionChanged(wxDataViewEvent& evt)
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
    if (uuids != cfg.GetSelectedTrusses()) {
        cfg.PushUndoState("truss selection");
        cfg.SetSelectedTrusses(uuids);
    }
    if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->SetSelectedFixtures(uuids);
    evt.Skip();
}

void TrussTablePanel::UpdateSceneData()
{
    ConfigManager& cfg = ConfigManager::Get();
    cfg.PushUndoState("edit truss");
    auto& scene = cfg.GetScene();
    size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());

    struct Dim { float len; float weight; };
    std::unordered_map<std::string, Dim> dims;

    auto makeKey = [](const std::string& n,
                      const std::string& m,
                      const std::string& mo) {
        return n + "\x1F" + m + "\x1F" + mo;
    };

    // First pass: update scene data from the table and track changed groups
    for (size_t i = 0; i < count; ++i)
    {
        auto it = scene.trusses.find(rowUuids[i]);
        if (it == scene.trusses.end())
            continue;

        Truss old = it->second;
        wxVariant v;

        table->GetValue(v, i, 0);
        it->second.name = std::string(v.GetString().mb_str());
        table->GetValue(v, i, 1);
        std::string layerStr = std::string(v.GetString().mb_str());
        if (layerStr.empty())
            it->second.layer.clear();
        else
            it->second.layer = layerStr;
        if (i < modelPaths.size())
            it->second.symbolFile = std::string(modelPaths[i].ToUTF8());
        else {
            table->GetValue(v, i, 2);
            it->second.symbolFile = std::string(v.GetString().ToUTF8());
        }
        table->GetValue(v, i, 3);
        it->second.positionName = std::string(v.GetString().mb_str());

        double x=0, y=0, z=0;
        table->GetValue(v, i, 4); v.GetString().ToDouble(&x);
        table->GetValue(v, i, 5); v.GetString().ToDouble(&y);
        table->GetValue(v, i, 6); v.GetString().ToDouble(&z);
        it->second.transform.o = {static_cast<float>(x * 1000.0),
                                  static_cast<float>(y * 1000.0),
                                  static_cast<float>(z * 1000.0)};

        table->GetValue(v, i, 10);
        it->second.manufacturer = std::string(v.GetString().mb_str());
        table->GetValue(v, i, 11);
        it->second.model = std::string(v.GetString().mb_str());

        double len=0.0, weight=0.0;
        table->GetValue(v, i, 12); v.GetString().ToDouble(&len);
        table->GetValue(v, i, 13); v.GetString().ToDouble(&weight);
        it->second.lengthMm = static_cast<float>(len * 1000.0);
        it->second.weightKg = static_cast<float>(weight);

        std::string key = makeKey(it->second.name,
                                  it->second.manufacturer,
                                  it->second.model);

        // If any relevant value changed, update canonical dimensions
        if (old.name != it->second.name ||
            old.manufacturer != it->second.manufacturer ||
            old.model != it->second.model ||
            old.lengthMm != it->second.lengthMm ||
            old.weightKg != it->second.weightKg)
        {
            dims[key] = {it->second.lengthMm, it->second.weightKg};
        }
        else if (!dims.count(key))
        {
            dims[key] = {it->second.lengthMm, it->second.weightKg};
        }
    }

    // Second pass: apply canonical dimensions to all members of each group
    for (size_t i = 0; i < count; ++i)
    {
        auto it = scene.trusses.find(rowUuids[i]);
        if (it == scene.trusses.end())
            continue;

        std::string key = makeKey(it->second.name,
                                  it->second.manufacturer,
                                  it->second.model);

        auto dit = dims.find(key);
        if (dit == dims.end())
            continue;

        float lenMm = dit->second.len;
        float weightKg = dit->second.weight;

        if (it->second.lengthMm != lenMm || it->second.weightKg != weightKg)
        {
            it->second.lengthMm = lenMm;
            it->second.weightKg = weightKg;
            wxString lenStr = wxString::Format("%.2f", lenMm / 1000.0f);
            wxString weiStr = wxString::Format("%.2f", weightKg);
            table->SetValue(wxVariant(lenStr), i, 12);
            table->SetValue(wxVariant(weiStr), i, 13);
        }
    }
}

TrussTablePanel* TrussTablePanel::Instance()
{
    return s_instance;
}

void TrussTablePanel::SetInstance(TrussTablePanel* panel)
{
    s_instance = panel;
}

bool TrussTablePanel::IsActivePage() const
{
    auto* nb = dynamic_cast<wxNotebook*>(GetParent());
    return nb && nb->GetPage(nb->GetSelection()) == this;
}

void TrussTablePanel::HighlightTruss(const std::string& uuid)
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

void TrussTablePanel::ClearSelection() {
    table->UnselectAll();
}

std::vector<std::string> TrussTablePanel::GetSelectedUuids() const {
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

void TrussTablePanel::SelectByUuid(const std::vector<std::string>& uuids) {
    table->UnselectAll();
    for (const auto& u : uuids) {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), u);
        if (pos != rowUuids.end())
            table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
    }
}

void TrussTablePanel::DeleteSelected()
{
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        return;

    ConfigManager& cfg = ConfigManager::Get();
    cfg.PushUndoState("delete truss");

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
            scene.trusses.erase(rowUuids[r]);
            rowUuids.erase(rowUuids.begin() + r);
            if ((size_t)r < modelPaths.size())
                modelPaths.erase(modelPaths.begin() + r);
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

void TrussTablePanel::ResyncRows(const std::vector<std::string>& oldOrder,
                                 const std::vector<std::string>& selectedUuids)
{
    unsigned int count = table->GetItemCount();
    std::vector<std::string> newOrder(count);
    std::vector<wxString> newPaths(count);
    for (unsigned int i = 0; i < count; ++i)
    {
        wxDataViewItem it = table->RowToItem(i);
        unsigned long idx = store.GetItemData(it);
        if (idx < oldOrder.size()) {
            newOrder[i] = oldOrder[idx];
            if (idx < modelPaths.size())
                newPaths[i] = modelPaths[idx];
        }
        store.SetItemData(it, i);
    }
    rowUuids.swap(newOrder);
    modelPaths.swap(newPaths);

    table->UnselectAll();
    for (const auto& uuid : selectedUuids)
    {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
        if (pos != rowUuids.end())
            table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
    }
}

void TrussTablePanel::OnColumnSorted(wxDataViewEvent& event)
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
