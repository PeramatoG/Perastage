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
#include "selection_origin_token.h"
#include "guiconfigservices.h"
#include "layerpanel.h"
#include "matrixutils.h"
#include "primitive_model_resources.h"
#include "projectutils.h"
#include "resource_reference_sync.h"
#include "scene_object_primitive_editing.h"
#include "stringutils.h"
#include "summarypanel.h"
#include "layoutviewerpanel.h"
#include "dataview_edit_commit.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include "units/unit_label_utils.h"
#include "units/units.h"
#include <wx/aui/aui.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <memory>
#include <wx/notebook.h>
#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/wupdlock.h> // freeze/thaw UI during batch edits
#include <wx/version.h>

static SceneObjectTablePanel* s_instance = nullptr;

namespace {

const wxString &DegreeSymbol() {
  static const wxString kDegreeSymbol = wxString::FromUTF8("\xC2\xB0");
  return kDegreeSymbol;
}

Units::DistanceUnitSystem ResolveDistanceUnitSystem() {
    auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    return Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
}

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

LayoutViewerPanel *FindLayoutViewerPanel(wxWindow *root) {
    if (!root)
        return nullptr;

    if (auto *layoutViewer = dynamic_cast<LayoutViewerPanel *>(root))
        return layoutViewer;

    for (wxWindow *child : root->GetChildren()) {
        if (auto *layoutViewer = FindLayoutViewerPanel(child))
            return layoutViewer;
    }
    return nullptr;
}

void RefreshSceneObjectVisuals() {
    if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->InvalidateBottomSymbolCache();
        Viewer2DPanel::Instance()->UpdateScene();
        Viewer2DPanel::Instance()->Refresh();
    }

    wxWindow *topLevel = wxGetTopLevelParent(SceneObjectTablePanel::Instance());
    if (auto *layoutViewer = FindLayoutViewerPanel(topLevel)) {
        layoutViewer->RefreshAfterSceneContentUpdate();
    }
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

std::string ModelRefForDisplay(const SceneObject &object)
{
    const std::string primary = object.GetPrimaryModel();
    if (primary.rfind("primitive:", 0) == 0) {
        const std::string archivePath = mvr::PrimitiveArchivePathForToken(primary, object.uuid);
        if (!archivePath.empty())
            return std::filesystem::path(archivePath).filename().string();
    }
    return primary;
}

wxString ResolvePrimitivePreviewPath(const SceneObject &object,
                                     const std::string &basePath)
{
    const std::string token = object.GetPrimaryModel();
    std::string archiveRel = mvr::PrimitiveArchivePathForToken(token, object.uuid);
    if (archiveRel.empty())
        archiveRel = mvr::PrimitiveArchivePathForToken(token);

    if (!archiveRel.empty() && !basePath.empty()) {
        std::filesystem::path candidate = std::filesystem::path(basePath) /
                                          std::filesystem::path(archiveRel);
        if (std::filesystem::exists(candidate))
            return wxString::FromUTF8(candidate.string());
    }

    std::filesystem::path cacheDir = std::filesystem::temp_directory_path() /
                                     "perastage_primitive_preview";
    std::error_code ec;
    std::filesystem::create_directories(cacheDir, ec);
    std::string fileName = std::filesystem::path(
        mvr::PrimitiveArchivePathForToken(token, object.uuid)).filename().string();
    if (fileName.empty())
        fileName = std::filesystem::path(ModelRefForDisplay(object)).string();
    std::filesystem::path outPath = cacheDir / std::filesystem::path(fileName);
    if (!std::filesystem::exists(outPath) &&
        !mvr::WritePrimitiveModelForToken(token, outPath.string()))
        return wxString();
    return wxString::FromUTF8(outPath.string());
}
} // namespace

SceneObjectTablePanel::SceneObjectTablePanel(wxWindow* parent, IGuiConfigServices* services)
    : wxPanel(parent, wxID_ANY), guiConfigServices(services ? services : &GetDefaultGuiConfigServices())
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
    table->Bind(wxEVT_LEFT_DCLICK, &SceneObjectTablePanel::OnLeftDClick, this);
    table->Bind(wxEVT_LEFT_UP, &SceneObjectTablePanel::OnLeftUp, this);
    table->Bind(wxEVT_MOTION, &SceneObjectTablePanel::OnMouseMove, this);
    table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
                &SceneObjectTablePanel::OnSelectionChanged, this);

    table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
                &SceneObjectTablePanel::OnContextMenu, this);
    table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED,
                &SceneObjectTablePanel::OnColumnSorted, this);
    table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                &SceneObjectTablePanel::OnItemActivated, this);

    Bind(wxEVT_MOUSE_CAPTURE_LOST, &SceneObjectTablePanel::OnCaptureLost, this);

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

// Releases table resources and detaches the scene-object pane from AUI layout management.
SceneObjectTablePanel::~SceneObjectTablePanel()
{
    if (wxAuiManager *manager = wxAuiManager::GetManager(this))
        manager->DetachPane(this);
    store = nullptr;
}

void SceneObjectTablePanel::InitializeTable()
{
    const auto distanceUnit = ResolveDistanceUnitSystem();
    const wxString distanceSuffix = wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnit));
    columnLabels = {"Name", "Layer", "Model File",
                    wxString::FromUTF8(Units::LabelWithUnit("Pos X", std::string(distanceSuffix.ToUTF8()))), wxString::FromUTF8(Units::LabelWithUnit("Pos Y", std::string(distanceSuffix.ToUTF8()))), wxString::FromUTF8(Units::LabelWithUnit("Pos Z", std::string(distanceSuffix.ToUTF8()))),
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
    const auto distanceUnit = ResolveDistanceUnitSystem();
    const wxString distanceSuffix =
        wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnit));
    columnLabels[3] = wxString::FromUTF8(Units::LabelWithUnit("Pos X", std::string(distanceSuffix.ToUTF8())));
    columnLabels[4] = wxString::FromUTF8(Units::LabelWithUnit("Pos Y", std::string(distanceSuffix.ToUTF8())));
    columnLabels[5] = wxString::FromUTF8(Units::LabelWithUnit("Pos Z", std::string(distanceSuffix.ToUTF8())));
    for (size_t i = 0; i < columnLabels.size(); ++i) {
        if (auto *column = table->GetColumn(static_cast<unsigned int>(i)))
            column->SetTitle(columnLabels[i]);
    }

    table->DeleteAllItems();
    modelPaths.clear();
    rowUuids.clear();
    rowUuidByKey.clear();
    modelPathByKey.clear();
    nextRowKey = 1;
    const auto& objs = guiConfigServices->LegacyConfigManager().GetScene().sceneObjects;

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
        const std::string primaryModel = obj.GetPrimaryModel();
        wxString model;
        wxString modelFullPath;
        const std::string &base = guiConfigServices->LegacyConfigManager().GetScene().basePath;
        if (primaryModel.rfind("primitive:", 0) == 0) {
            modelFullPath = ResolvePrimitivePreviewPath(obj, base);
            if (!modelFullPath.IsEmpty())
                model = wxFileName(modelFullPath).GetFullName();
            else
                model = wxString::FromUTF8(ModelRefForDisplay(obj));
        } else if (!primaryModel.empty()) {
            wxFileName fullPath(base.empty() ? wxString::FromUTF8(primaryModel)
                                             : wxString::FromUTF8((std::filesystem::path(base) /
                                                                   std::filesystem::path(primaryModel)).string()));
            modelFullPath = fullPath.GetFullPath();
            model = fullPath.GetFullName();
        }

        auto posArr = obj.transform.o;
        wxString posX = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
            posArr[0], distanceUnit, Units::ValueFormatContext::Table));
        wxString posY = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
            posArr[1], distanceUnit, Units::ValueFormatContext::Table));
        wxString posZ = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
            posArr[2], distanceUnit, Units::ValueFormatContext::Table));

        auto euler = MatrixUtils::MatrixToEuler(obj.transform);
        wxString roll = wxString::Format("%.1f", euler[2]) + DegreeSymbol();
        wxString pitch = wxString::Format("%.1f", euler[1]) + DegreeSymbol();
        wxString yaw = wxString::Format("%.1f", euler[0]) + DegreeSymbol();

        row.push_back(name);
        row.push_back(layer);
        row.push_back(model);
        row.push_back(posX);
        row.push_back(posY);
        row.push_back(posZ);
        row.push_back(roll);
        row.push_back(pitch);
        row.push_back(yaw);

        const wxUIntPtr rowKey = nextRowKey++;
        store->AppendItem(row, rowKey);
        modelPaths.push_back(modelFullPath);
        rowUuids.push_back(uuid);
        rowUuidByKey[rowKey] = uuid;
        modelPathByKey[rowKey] = modelFullPath;
    }

    // Let wxDataViewListCtrl manage column headers and sorting
    if (LayerPanel::Instance())
        LayerPanel::Instance()->ReloadLayers();
    if (SummaryPanel::Instance() && IsActivePage())
        SummaryPanel::Instance()->ShowSceneObjectSummary();
}

// Handles context-menu edits and updates scene/rendering only when an actual table value changes.
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
        const std::string uuid = UuidForItem(it);
        if (!uuid.empty())
            selectedUuids.push_back(uuid);
    }
    std::vector<std::string> oldOrder = rowUuids;

    int row = table->ItemToRow(item);
    if (row == wxNOT_FOUND)
        return;

    wxVariant current;
    table->GetValue(current, row, col);
    
    if (col == 1)
    {
        auto layers = guiConfigServices->LegacyConfigManager().GetLayerNames();
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
        if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
        }
        return;
    }

    if (col == 2)
    {
        wxString initialDir = wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("objects"));
        if (row >= 0 && static_cast<size_t>(row) < modelPaths.size() && !modelPaths[static_cast<size_t>(row)].IsEmpty()) {
            wxFileName current(modelPaths[static_cast<size_t>(row)]);
            if (current.DirExists())
                initialDir = current.GetPath();
        }

        wxFileDialog fdlg(this, "Select Object Model", initialDir, wxEmptyString,
                          "3D files (*.glb;*.gltf;*.3ds;*.obj)|*.glb;*.gltf;*.3ds;*.obj|All files|*.*",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (fdlg.ShowModal() != wxID_OK)
            return;

        wxString selectedPath = fdlg.GetPath();
        wxString displayName = wxFileName(selectedPath).GetFullName();
        bool changed = false;
        for (const auto& itSel : selections)
        {
            int r = table->ItemToRow(itSel);
            if (r == wxNOT_FOUND)
                continue;
            if (static_cast<size_t>(r) >= modelPaths.size())
                modelPaths.resize(table->GetItemCount());
            const wxString previousPath = modelPaths[static_cast<size_t>(r)];
            wxVariant existingDisplayName;
            table->GetValue(existingDisplayName, r, col);
            if (existingDisplayName.GetString() != displayName) {
                table->SetValue(wxVariant(displayName), r, col);
                changed = true;
            }
            SetModelPathForRow(static_cast<unsigned int>(r), selectedPath);
            if (previousPath != selectedPath)
                changed = true;
        }

        if (!changed)
            return;
        ResyncRows(oldOrder, selectedUuids);
        modelFileEditCommitPending = true;
        UpdateSceneData();
        modelFileEditCommitPending = false;
        if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
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
                if (col >= 6) {
                    if (!DegreeSymbol().empty())
                        cur.Replace(DegreeSymbol(), "");
                }
                double curVal = 0.0;
                cur.ToDouble(&curVal);
                double newVal = curVal + delta;
                wxString out;
                if (col >= 6)
                    out = wxString::Format("%.1f", newVal) + DegreeSymbol();
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
                    out = wxString::Format("%.1f", val) + DegreeSymbol();
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
    if (Viewer3DPanel::Instance()) {
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

void SceneObjectTablePanel::OnLeftDClick(wxMouseEvent& evt)
{
    wxDataViewItem item;
    wxDataViewColumn* col;
    table->HitTest(evt.GetPosition(), item, col);
    const int row = table->ItemToRow(item);
    if (row == wxNOT_FOUND || static_cast<size_t>(row) >= rowUuids.size()) {
        evt.Skip();
        return;
    }

    const std::string uuid = rowUuids[static_cast<size_t>(row)];
    ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
    const bool edited = scene_object_primitives::EditPrimitiveObjectByUuid(
        this, cfg, uuid);
    if (edited) {
        if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
        } else if (Viewer2DPanel::Instance()) {
            Viewer2DPanel::Instance()->UpdateScene();
        }
        ReloadData();
        SelectByUuid({uuid});
        return;
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
    RebuildRowCachesFromRowKeys();
    const selection::Origin origin = selection::CurrentOrigin();
    if (origin == selection::Origin::Viewer2D ||
        origin == selection::Origin::Viewer3D) {
        UpdateSelectionHighlight();
        evt.Skip();
        return;
    }

    wxDataViewItemArray selections;
    table->GetSelections(selections);
    std::vector<std::string> uuids;
    uuids.reserve(selections.size());
    for (const auto& it : selections)
    {
        const std::string uuid = UuidForItem(it);
        if (!uuid.empty())
            uuids.push_back(uuid);
    }
    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
    if (uuids != cfg.GetSelectedSceneObjects()) {
        cfg.PushUndoState("scene object selection");
        cfg.SetSelectedSceneObjects(uuids);
    }
    std::vector<std::string> mergedSelection;
    const auto appendSelection = [&](const std::vector<std::string>& source) {
        mergedSelection.insert(mergedSelection.end(), source.begin(), source.end());
    };
    appendSelection(cfg.GetSelectedFixtures());
    appendSelection(cfg.GetSelectedTrusses());
    appendSelection(cfg.GetSelectedSupports());
    appendSelection(cfg.GetSelectedSceneObjects());
    selection::ScopedOrigin selectionOrigin(selection::Origin::Table);
    if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->SetSelectedFixtures(mergedSelection);
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->SetSelectedUuids(mergedSelection);
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

    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
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

// Applies edited scene object table values back into the scene data model.
void SceneObjectTablePanel::UpdateSceneData(bool logChanges)
{
    // Ensure in-place cell editors commit pending values before reading table rows.
    if (table)
        DataViewEditCommit::CommitPendingEdit(table);


    (void)logChanges;
    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
    auto& scene = cfg.GetScene();
    size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());
    bool anyChanged = false;
    bool undoPushed = false;
    auto pushUndoIfNeeded = [&]() {
        if (!undoPushed)
        {
            cfg.PushUndoState("edit scene object");
            undoPushed = true;
        }
    };

    // Persist row values only when editable scene object data differs from scene data.
    for (size_t i = 0; i < count; ++i)
    {
        auto it = scene.sceneObjects.find(rowUuids[i]);
        if (it == scene.sceneObjects.end())
            continue;

        const SceneObject old = it->second;
        SceneObject next = old;
        wxVariant v;

        table->GetValue(v, i, 0);
        next.name = std::string(v.GetString().ToUTF8());

        table->GetValue(v, i, 1);
        std::string layerStr = std::string(v.GetString().ToUTF8());
        if (layerStr.empty())
            next.layer.clear();
        else
            next.layer = layerStr;

        const bool hasPrimitiveModel = old.GetPrimaryModel().rfind("primitive:", 0) == 0;
        if (!modelFileEditCommitPending)
            next.modelFile = old.modelFile;
        else if (hasPrimitiveModel)
            next.modelFile = old.modelFile;
        else if (i < modelPaths.size() && !modelPaths[i].IsEmpty())
            next.modelFile = gui::PreserveSceneResourceReferenceForTableSync(
                scene.basePath, old.modelFile, std::string(modelPaths[i].ToUTF8()),
                old.geometries.empty() ? std::string() : old.GetPrimaryModel());
        else {
            table->GetValue(v, i, 2);
            next.modelFile = std::string(v.GetString().ToUTF8());
        }

        const auto distanceUnit = ResolveDistanceUnitSystem();
        double xMm = old.transform.o[0], yMm = old.transform.o[1], zMm = old.transform.o[2];
        table->GetValue(v, i, 3);
        if (const auto parsed = Units::ParseDistanceToMillimeters(std::string(v.GetString().ToUTF8()), distanceUnit); parsed.has_value())
            xMm = *parsed;
        table->GetValue(v, i, 4);
        if (const auto parsed = Units::ParseDistanceToMillimeters(std::string(v.GetString().ToUTF8()), distanceUnit); parsed.has_value())
            yMm = *parsed;
        table->GetValue(v, i, 5);
        if (const auto parsed = Units::ParseDistanceToMillimeters(std::string(v.GetString().ToUTF8()), distanceUnit); parsed.has_value())
            zMm = *parsed;

        double roll = 0, pitch = 0, yaw = 0;
        table->GetValue(v, i, 6);
        {
            wxString s = v.GetString();
            if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
            s.ToDouble(&roll);
        }
        table->GetValue(v, i, 7);
        {
            wxString s = v.GetString();
            if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
            s.ToDouble(&pitch);
        }
        table->GetValue(v, i, 8);
        {
            wxString s = v.GetString();
            if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
            s.ToDouble(&yaw);
        }

        const auto currentEuler = MatrixUtils::MatrixToEuler(old.transform);
        const bool transformChanged =
            !Units::NearlyEqualDistanceMillimeters(old.transform.o[0], xMm, 0.5) ||
            !Units::NearlyEqualDistanceMillimeters(old.transform.o[1], yMm, 0.5) ||
            !Units::NearlyEqualDistanceMillimeters(old.transform.o[2], zMm, 0.5) ||
            std::abs(static_cast<double>(currentEuler[2]) - roll) > 0.05 ||
            std::abs(static_cast<double>(currentEuler[1]) - pitch) > 0.05 ||
            std::abs(static_cast<double>(currentEuler[0]) - yaw) > 0.05;

        if (transformChanged)
        {
            Matrix rot = MatrixUtils::EulerToMatrix(static_cast<float>(yaw),
                                                    static_cast<float>(pitch),
                                                    static_cast<float>(roll));
            next.transform = MatrixUtils::ApplyRotationPreservingScale(
                old.transform, rot,
                {static_cast<float>(xMm),
                 static_cast<float>(yMm),
                 static_cast<float>(zMm)});
        }

        const bool objectChanged = old.name != next.name ||
                                   old.layer != next.layer ||
                                   old.modelFile != next.modelFile ||
                                   transformChanged;
        if (!objectChanged)
            continue;

        pushUndoIfNeeded();
        anyChanged = true;
        it->second = next;
    }

    if (!anyChanged)
        return;

    if (SummaryPanel::Instance() && IsActivePage())
        SummaryPanel::Instance()->ShowSceneObjectSummary();

    RefreshSceneObjectVisuals();
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

// Applies a primary hover highlight to one scene object row.
void SceneObjectTablePanel::HighlightObject(const std::string& uuid)
{
    HighlightObject(uuid, {});
}

// Applies primary and related group-hover highlights to scene object rows.
void SceneObjectTablePanel::HighlightObject(
    const std::string& uuid, const std::vector<std::string>& relatedUuids)
{
    if (uuid == highlightedUuid && relatedUuids == highlightedRelatedUuids)
        return;

    auto findRow = [&](const std::string& candidate) -> int {
        if (candidate.empty())
            return wxNOT_FOUND;
        auto it = std::find(rowUuids.begin(), rowUuids.end(), candidate);
        if (it == rowUuids.end())
            return wxNOT_FOUND;
        int row = static_cast<int>(std::distance(rowUuids.begin(), it));
        if (row < 0 || row >= static_cast<int>(table->GetItemCount()))
            return wxNOT_FOUND;
        return row;
    };

    std::vector<bool> primaryRows(table->GetItemCount(), false);
    std::vector<bool> secondaryRows(table->GetItemCount(), false);
    const int currentRow = findRow(uuid);
    if (currentRow != wxNOT_FOUND)
        primaryRows[static_cast<size_t>(currentRow)] = true;
    for (const auto& relatedUuid : relatedUuids) {
        const int relatedRow = findRow(relatedUuid);
        if (relatedRow != wxNOT_FOUND && relatedRow != currentRow)
            secondaryRows[static_cast<size_t>(relatedRow)] = true;
    }
    store->SetHighlightRows(primaryRows, secondaryRows, wxColour(170, 220, 0),
                            wxColour(110, 210, 150), wxColour(0, 0, 0));

    highlightedUuid = uuid;
    highlightedRelatedUuids = relatedUuids;
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
        const wxUIntPtr rowKey = store->GetItemData(it);
        auto keyIt = rowUuidByKey.find(rowKey);
        if (keyIt != rowUuidByKey.end())
            uuids.push_back(keyIt->second);
    }
    return uuids;
}

void SceneObjectTablePanel::SelectByUuid(const std::vector<std::string>& uuids,
                                         bool notifySelectionChanged) {
    RebuildRowCachesFromRowKeys();
    std::unique_ptr<wxEventBlocker> selectionBlocker;
    if (!notifySelectionChanged) {
        selectionBlocker = std::make_unique<wxEventBlocker>(
            table, wxEVT_DATAVIEW_SELECTION_CHANGED);
    }
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

void SceneObjectTablePanel::DeleteSelected(bool pushUndoState)
{
    RebuildRowCachesFromRowKeys();
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        return;

    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
    if (pushUndoState)
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
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    std::vector<std::string> uuidsToDelete;
    uuidsToDelete.reserve(rows.size());
    for (int r : rows) {
        if (r >= 0 && static_cast<size_t>(r) < rowUuids.size())
            uuidsToDelete.push_back(rowUuids[r]);
    }

    auto& scene = guiConfigServices->LegacyConfigManager().GetScene();
    for (const auto& uuid : uuidsToDelete)
        scene.sceneObjects.erase(uuid);

    for (int r : rows) {
        if ((size_t)r < rowUuids.size()) {
            wxDataViewItem rowItem = table->RowToItem(static_cast<unsigned int>(r));
            const wxUIntPtr rowKey = store->GetItemData(rowItem);
            rowUuids.erase(rowUuids.begin() + r);
            rowUuidByKey.erase(rowKey);
            modelPathByKey.erase(rowKey);
            table->DeleteItem(r);
        }
    }

    std::vector<std::string> mergedSelection;
    const auto appendSelection = [&](const std::vector<std::string>& source) {
        mergedSelection.insert(mergedSelection.end(), source.begin(), source.end());
    };
    appendSelection(cfg.GetSelectedFixtures());
    appendSelection(cfg.GetSelectedTrusses());
    appendSelection(cfg.GetSelectedSupports());
    appendSelection(cfg.GetSelectedSceneObjects());

    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->SetSelectedFixtures(mergedSelection);
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
    else if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->SetSelectedUuids(mergedSelection);
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
    (void)oldOrder;
    RebuildRowCachesFromRowKeys();

    table->UnselectAll();
    for (const auto& uuid : selectedUuids)
    {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
        if (pos != rowUuids.end())
            table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
    }
    UpdateSelectionHighlight();
}

void SceneObjectTablePanel::RebuildRowCachesFromRowKeys() {
    if (!table || !store)
        return;
    const unsigned int count = table->GetItemCount();
    rowUuids.assign(count, std::string());
    modelPaths.assign(count, wxString());
    for (unsigned int row = 0; row < count; ++row) {
        wxDataViewItem item = table->RowToItem(row);
        const wxUIntPtr rowKey = store->GetItemData(item);
        auto uuidIt = rowUuidByKey.find(rowKey);
        if (uuidIt != rowUuidByKey.end())
            rowUuids[row] = uuidIt->second;
        auto pathIt = modelPathByKey.find(rowKey);
        if (pathIt != modelPathByKey.end())
            modelPaths[row] = pathIt->second;
    }
}

std::string SceneObjectTablePanel::UuidForItem(const wxDataViewItem& item) const {
    if (!store || !item.IsOk())
        return {};
    const wxUIntPtr rowKey = store->GetItemData(item);
    auto it = rowUuidByKey.find(rowKey);
    if (it == rowUuidByKey.end())
        return {};
    return it->second;
}

void SceneObjectTablePanel::SetModelPathForRow(unsigned int row,
                                               const wxString& modelPath) {
    if (!table || !store || row >= table->GetItemCount())
        return;
    if (row >= modelPaths.size())
        modelPaths.resize(table->GetItemCount());
    modelPaths[row] = modelPath;
    wxDataViewItem item = table->RowToItem(row);
    const wxUIntPtr rowKey = store->GetItemData(item);
    modelPathByKey[rowKey] = modelPath;
}

void SceneObjectTablePanel::OnColumnSorted(wxDataViewEvent& event)
{
    RebuildRowCachesFromRowKeys();
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    std::vector<std::string> selectedUuids;
    for (const auto& it : selections)
    {
        const std::string uuid = UuidForItem(it);
        if (!uuid.empty())
            selectedUuids.push_back(uuid);
    }
    std::vector<std::string> oldOrder = rowUuids;
    ResyncRows(oldOrder, selectedUuids);
    event.Skip();
}

void SceneObjectTablePanel::OnItemActivated(wxDataViewEvent &event)
{
    const wxDataViewItem item = event.GetItem();
    const int row = table->ItemToRow(item);
    if (row == wxNOT_FOUND || static_cast<size_t>(row) >= rowUuids.size()) {
        event.Skip();
        return;
    }

    const std::string uuid = rowUuids[static_cast<size_t>(row)];
    ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
    const bool edited = scene_object_primitives::EditPrimitiveObjectByUuid(
        this, cfg, uuid);
    if (!edited)
        return;

    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    } else if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->UpdateScene();
    }

    ReloadData();
    SelectByUuid({uuid});
}
