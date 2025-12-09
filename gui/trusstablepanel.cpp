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
#include "trusstablepanel.h"
#include "columnutils.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "layerpanel.h"
#include "matrixutils.h"
#include "projectutils.h"
#include "riggingpanel.h"
#include "stringutils.h"
#include "summarypanel.h"
#include "trussdictionary.h"
#include "trussloader.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
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
    store = new ColorfulDataViewListStore();
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, wxDV_MULTIPLE | wxDV_ROW_LINES);
    table->AssociateModel(store);
    store->DecRef();

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

    Bind(wxEVT_MOUSE_CAPTURE_LOST, &TrussTablePanel::OnCaptureLost, this);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

TrussTablePanel::~TrussTablePanel()
{
    store = nullptr;
}

void TrussTablePanel::InitializeTable()
{
    columnLabels = {"Name", "Layer", "Model File", "Hang Pos",
                    "Pos X", "Pos Y", "Pos Z",
                    "Roll (X)", "Pitch (Y)", "Yaw (Z)",
                    "Manufacturer", "Model",
                    "Length (m)", "Width (m)", "Height (m)", "Weight (kg)"};
    std::vector<int> widths = {150, 100, 180, 120,
                               80, 80, 80,
                               80, 80, 80,
                               120, 120,
                               90, 90, 90, 90};
    for (size_t i = 0; i < columnLabels.size(); ++i)
        table->AppendTextColumn(
            columnLabels[i], wxDATAVIEW_CELL_INERT, widths[i], wxALIGN_LEFT,
            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    ColumnUtils::EnforceMinColumnWidth(table);
}

void TrussTablePanel::ReloadData()
{
    table->DeleteAllItems();
    rowUuids.clear();
    modelPaths.clear();
    symbolPaths.clear();
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
        std::string displayPath;
        std::string symbolFullPath;
        const std::string &base = ConfigManager::Get().GetScene().basePath;
        if (!truss.modelFile.empty()) {
            fs::path p = base.empty() ? fs::path(truss.modelFile)
                                     : fs::path(base) / truss.modelFile;
            displayPath = p.string();
        } else if (!truss.symbolFile.empty()) {
            fs::path p = base.empty() ? fs::path(truss.symbolFile)
                                     : fs::path(base) / truss.symbolFile;
            displayPath = p.string();
        }
        if (!truss.symbolFile.empty()) {
            fs::path p = base.empty() ? fs::path(truss.symbolFile)
                                     : fs::path(base) / truss.symbolFile;
            symbolFullPath = p.string();
        }
        wxString modelFull = wxString::FromUTF8(displayPath);
        modelPaths.push_back(modelFull);
        symbolPaths.push_back(wxString::FromUTF8(symbolFullPath));
        wxString model = wxFileName(modelFull).GetFullName();

        auto posArr = truss.transform.o;
        wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
        wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
        wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);

        auto euler = MatrixUtils::MatrixToEuler(truss.transform);
        wxString roll = wxString::Format("%.1f\u00B0", euler[2]);
        wxString pitch = wxString::Format("%.1f\u00B0", euler[1]);
        wxString yaw = wxString::Format("%.1f\u00B0", euler[0]);

        row.push_back(name);
        row.push_back(layer);
        row.push_back(model);
        wxString posName = wxString::FromUTF8(truss.positionName);
        row.push_back(posName);
        row.push_back(posX);
        row.push_back(posY);
        row.push_back(posZ);
        row.push_back(roll);
        row.push_back(pitch);
        row.push_back(yaw);
        wxString manuf = wxString::FromUTF8(truss.manufacturer);
        wxString modelName = wxString::FromUTF8(truss.model);
        wxString len = wxString::Format("%.2f", truss.lengthMm / 1000.0f);
        wxString wid = truss.widthMm > 0.0f
                           ? wxString::Format("%.2f", truss.widthMm / 1000.0f)
                           : wxString();
        wxString hei = truss.heightMm > 0.0f
                            ? wxString::Format("%.2f", truss.heightMm / 1000.0f)
                            : wxString();
        wxString weight = wxString::Format("%.2f", truss.weightKg);
        row.push_back(manuf);
        row.push_back(modelName);
        row.push_back(len);
        row.push_back(wid);
        row.push_back(hei);
        row.push_back(weight);

        store->AppendItem(row, rowUuids.size());
        rowUuids.push_back(uuid);
    }

    // Let wxDataViewListCtrl manage column headers and sorting
    if (LayerPanel::Instance())
        LayerPanel::Instance()->ReloadLayers();
    if (SummaryPanel::Instance() && IsActivePage())
        SummaryPanel::Instance()->ShowTrussSummary();
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
        else if (Viewer2DPanel::Instance())
        {
            Viewer2DPanel::Instance()->UpdateScene();
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
            std::string archivePath(selPath.ToUTF8());
            std::string geomPath = archivePath;
            Truss parsed;
            bool parsedOk = false;
            wxString manuf, modelNameWx, lenStr, widStr, heiStr, weightStr;
            std::string modelKey;

            // Remember the existing model name so the dictionary maps
            // rider-provided names to the selected model file.  This is
            // read before any table values are overwritten by parsed data.
            {
                wxVariant mv;
                table->GetValue(mv, row, 11);
                modelNameWx = mv.GetString();
                modelKey = std::string(modelNameWx.ToUTF8());
            }

            if (fs::path(archivePath).extension() == ".gtruss" &&
                LoadTrussArchive(archivePath, parsed))
            {
                geomPath = parsed.symbolFile;
                manuf = wxString::FromUTF8(parsed.manufacturer);
                modelNameWx = wxString::FromUTF8(parsed.model);
                lenStr = wxString::Format("%.2f", parsed.lengthMm / 1000.0f);
                widStr = parsed.widthMm > 0.0f
                              ? wxString::Format("%.2f", parsed.widthMm / 1000.0f)
                              : wxString();
                heiStr = parsed.heightMm > 0.0f
                               ? wxString::Format("%.2f", parsed.heightMm / 1000.0f)
                               : wxString();
                weightStr = wxString::Format("%.2f", parsed.weightKg);
                parsedOk = true;
            }
            wxString fileName =
                wxFileName(wxString::FromUTF8(archivePath)).GetFullName();
            if (modelPaths.size() < table->GetItemCount())
                modelPaths.resize(table->GetItemCount());
            if (symbolPaths.size() < table->GetItemCount())
                symbolPaths.resize(table->GetItemCount());
            for (const auto& itSel : selections)
            {
                int r = table->ItemToRow(itSel);
                if (r == wxNOT_FOUND)
                    continue;
                modelPaths[r] = wxString::FromUTF8(archivePath);
                symbolPaths[r] = wxString::FromUTF8(geomPath);
                table->SetValue(wxVariant(fileName), r, 2);
                if (parsedOk)
                {
                    table->SetValue(wxVariant(manuf), r, 10);
                    table->SetValue(wxVariant(modelNameWx), r, 11);
                    table->SetValue(wxVariant(lenStr), r, 12);
                    table->SetValue(wxVariant(widStr), r, 13);
                    table->SetValue(wxVariant(heiStr), r, 14);
                    table->SetValue(wxVariant(weightStr), r, 15);
                }
            }
            // Apply the model file to any other rows that share the original
            // model name from the rider.  This lets multiple trusses with the
            // same rider-specified model get updated in one action and ensures
            // the dictionary entry uses that rider key.
            for (unsigned int i = 0; i < table->GetItemCount(); ++i)
            {
                wxVariant mv;
                table->GetValue(mv, i, 11);
                if (mv.GetString() == wxString::FromUTF8(modelKey))
                {
                    modelPaths[i] = wxString::FromUTF8(archivePath);
                    symbolPaths[i] = wxString::FromUTF8(geomPath);
                    table->SetValue(wxVariant(fileName), i, 2);
                    if (parsedOk)
                    {
                        table->SetValue(wxVariant(manuf), i, 10);
                        table->SetValue(wxVariant(modelNameWx), i, 11);
                        table->SetValue(wxVariant(lenStr), i, 12);
                        table->SetValue(wxVariant(widStr), i, 13);
                        table->SetValue(wxVariant(heiStr), i, 14);
                        table->SetValue(wxVariant(weightStr), i, 15);
                    }
                }
            }
            TrussDictionary::Update(modelKey, archivePath);
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
        return;
    }

    wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col], current.GetString());
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString value = dlg.GetValue().Trim(true).Trim(false);

    bool numericCol = (col >= 4 && col <= 9);
    bool relative = false;
    double delta = 0.0;
    if (numericCol && (value.StartsWith("++") || value.StartsWith("--")))
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
    else if (Viewer2DPanel::Instance())
    {
        Viewer2DPanel::Instance()->UpdateScene();
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

void TrussTablePanel::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(evt))
{
    dragSelecting = false;
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

    struct Dim {
        float len;
        float wid;
        float hei;
        float weight;
    };
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
        if (i < symbolPaths.size())
            it->second.symbolFile = std::string(symbolPaths[i].ToUTF8());
        else if (i < modelPaths.size())
            it->second.symbolFile = std::string(modelPaths[i].ToUTF8());
        else {
            table->GetValue(v, i, 2);
            it->second.symbolFile = std::string(v.GetString().ToUTF8());
        }
        if (i < modelPaths.size())
            it->second.modelFile = std::string(modelPaths[i].ToUTF8());
        else {
            table->GetValue(v, i, 2);
            it->second.modelFile = std::string(v.GetString().ToUTF8());
        }
        table->GetValue(v, i, 3);
        it->second.positionName = std::string(v.GetString().mb_str());
        if (!it->second.position.empty())
            scene.positions[it->second.position] = it->second.positionName;

        double x=0, y=0, z=0;
        table->GetValue(v, i, 4); v.GetString().ToDouble(&x);
        table->GetValue(v, i, 5); v.GetString().ToDouble(&y);
        table->GetValue(v, i, 6); v.GetString().ToDouble(&z);

        double roll=0, pitch=0, yaw=0;
        table->GetValue(v, i, 7); {
            wxString s = v.GetString(); s.Replace("\u00B0", ""); s.ToDouble(&roll);
        }
        table->GetValue(v, i, 8); {
            wxString s = v.GetString(); s.Replace("\u00B0", ""); s.ToDouble(&pitch);
        }
        table->GetValue(v, i, 9); {
            wxString s = v.GetString(); s.Replace("\u00B0", ""); s.ToDouble(&yaw);
        }

        Matrix rot = MatrixUtils::EulerToMatrix(static_cast<float>(yaw),
                                                static_cast<float>(pitch),
                                                static_cast<float>(roll));
        rot.o = {static_cast<float>(x * 1000.0),
                 static_cast<float>(y * 1000.0),
                 static_cast<float>(z * 1000.0)};
        it->second.transform = rot;

        table->GetValue(v, i, 10);
        it->second.manufacturer = std::string(v.GetString().mb_str());
        table->GetValue(v, i, 11);
        it->second.model = std::string(v.GetString().mb_str());

        double len=0.0, wid=0.0, hei=0.0, weight=0.0;
        table->GetValue(v, i, 12); v.GetString().ToDouble(&len);
        table->GetValue(v, i, 13); v.GetString().ToDouble(&wid);
        table->GetValue(v, i, 14); v.GetString().ToDouble(&hei);
        table->GetValue(v, i, 15); v.GetString().ToDouble(&weight);
        it->second.lengthMm = static_cast<float>(len * 1000.0);
        it->second.widthMm = static_cast<float>(wid * 1000.0);
        it->second.heightMm = static_cast<float>(hei * 1000.0);
        it->second.weightKg = static_cast<float>(weight);

        std::string key = makeKey(it->second.name,
                                  it->second.manufacturer,
                                  it->second.model);

        // If any relevant value changed, update canonical dimensions
        if (old.name != it->second.name ||
            old.manufacturer != it->second.manufacturer ||
            old.model != it->second.model ||
            old.lengthMm != it->second.lengthMm ||
            old.widthMm != it->second.widthMm ||
            old.heightMm != it->second.heightMm ||
            old.weightKg != it->second.weightKg)
        {
            dims[key] = {it->second.lengthMm, it->second.widthMm,
                         it->second.heightMm, it->second.weightKg};
        }
        else if (!dims.count(key))
        {
            dims[key] = {it->second.lengthMm, it->second.widthMm,
                         it->second.heightMm, it->second.weightKg};
        }

        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format(
                "Updated truss %s (UUID %s)",
                wxString::FromUTF8(it->second.name.c_str()),
                wxString::FromUTF8(it->second.uuid.c_str()));
            ConsolePanel::Instance()->AppendMessage(msg);
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
        float widMm = dit->second.wid;
        float heiMm = dit->second.hei;
        float weightKg = dit->second.weight;

        if (it->second.lengthMm != lenMm || it->second.widthMm != widMm ||
            it->second.heightMm != heiMm || it->second.weightKg != weightKg)
        {
            it->second.lengthMm = lenMm;
            it->second.widthMm = widMm;
            it->second.heightMm = heiMm;
            it->second.weightKg = weightKg;
            wxString lenStr = wxString::Format("%.2f", lenMm / 1000.0f);
            wxString widStr = widMm > 0.0f
                                   ? wxString::Format("%.2f", widMm / 1000.0f)
                                   : wxString();
            wxString heiStr = heiMm > 0.0f
                                   ? wxString::Format("%.2f", heiMm / 1000.0f)
                                   : wxString();
            wxString weiStr = wxString::Format("%.2f", weightKg);
            table->SetValue(wxVariant(lenStr), i, 12);
            table->SetValue(wxVariant(widStr), i, 13);
            table->SetValue(wxVariant(heiStr), i, 14);
            table->SetValue(wxVariant(weiStr), i, 15);
        }
    }

    if (SummaryPanel::Instance() && IsActivePage())
        SummaryPanel::Instance()->ShowTrussSummary();

    if (RiggingPanel::Instance())
        RiggingPanel::Instance()->RefreshData();
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
            store->SetRowBackgroundColour(i, wxColour(0, 200, 0));
        else
            store->ClearRowBackground(i);
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
            if ((size_t)r < symbolPaths.size())
                symbolPaths.erase(symbolPaths.begin() + r);
            table->DeleteItem(r);
        }
    }

    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
    else if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->UpdateScene();
    }

    if (SummaryPanel::Instance())
        SummaryPanel::Instance()->ShowTrussSummary();

    std::vector<std::string> order = rowUuids;
    ResyncRows(order, {});
}

void TrussTablePanel::ResyncRows(const std::vector<std::string>& oldOrder,
                                 const std::vector<std::string>& selectedUuids)
{
    unsigned int count = table->GetItemCount();
    std::vector<std::string> newOrder(count);
    std::vector<wxString> newPaths(count);
    std::vector<wxString> newSymPaths(count);
    for (unsigned int i = 0; i < count; ++i)
    {
        wxDataViewItem it = table->RowToItem(i);
        unsigned long idx = store->GetItemData(it);
        if (idx < oldOrder.size()) {
            newOrder[i] = oldOrder[idx];
            if (idx < modelPaths.size())
                newPaths[i] = modelPaths[idx];
            if (idx < symbolPaths.size())
                newSymPaths[i] = symbolPaths[idx];
        }
        store->SetItemData(it, i);
    }
    rowUuids.swap(newOrder);
    modelPaths.swap(newPaths);
    symbolPaths.swap(newSymPaths);

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
