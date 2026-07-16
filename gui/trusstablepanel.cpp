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
#include "dataview_deferred_selection_guard.h"
#include "localized_unit_labels.h"
#include "columnutils.h"
#include "colorfulrenderers.h"
#include "configmanager.h"
#include "selection_origin_token.h"
#include "scene_view_refresh.h"
#include "guiconfigservices.h"
#include "hang_position_dialog.h"
#include "consolepanel.h"
#include "hoist_load_recalculation_prompt.h"
#include "layerpanel.h"
#include "matrixutils.h"
#include "projectutils.h"
#include "resource_reference_sync.h"
#include "riggingpanel.h"
#include "stringutils.h"
#include "summarypanel.h"
#include "table_column_indices.h"
#include "dataview_edit_commit.h"
#include "trussdictionary.h"
#include "trusseditdialog.h"
#include "trussloader.h"
#include "units/unit_label_utils.h"
#include "units/units.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <wx/aui/aui.h>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <memory>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <wx/notebook.h>
#include <wx/choicdlg.h>
#include <wx/wupdlock.h> // freeze/thaw UI during batch edits
#include <wx/version.h>

static TrussTablePanel* s_instance = nullptr;
namespace fs = std::filesystem;

namespace {

using TrussColumn = TrussTableColumns::Column;

// Converts a truss column to its stable model index.
constexpr int ColumnIndex(TrussColumn column) {
    return TableColumnIndices::ToIndex(column);
}

// Checks whether a truss column contains editable transform data.
bool IsTransformColumn(TrussColumn column) {
    return column >= TrussColumn::PositionX && column <= TrussColumn::Yaw;
}

// Checks whether a truss column contains rotation data.
bool IsRotationColumn(TrussColumn column) {
    return column >= TrussColumn::Roll && column <= TrussColumn::Yaw;
}

// Checks whether a truss column stores shared truss type dimensions.
bool IsSharedTrussTypeDimensionColumn(TrussColumn column) {
    return column == TrussColumn::Length || column == TrussColumn::Width ||
           column == TrussColumn::Height || column == TrussColumn::Weight;
}

// Builds the table key used to match visible truss type rows.
std::string BuildVisibleTrussTypeKey(wxDataViewListCtrl *table, int row) {
    if (!table || row == wxNOT_FOUND)
        return {};

    wxVariant name;
    wxVariant manufacturer;
    wxVariant model;
    table->GetValue(name, row, ColumnIndex(TrussColumn::Name));
    table->GetValue(manufacturer, row, ColumnIndex(TrussColumn::Manufacturer));
    table->GetValue(model, row, ColumnIndex(TrussColumn::Model));
    return std::string(name.GetString().ToUTF8()) + "" +
           std::string(manufacturer.GetString().ToUTF8()) + "" +
           std::string(model.GetString().ToUTF8());
}

// Propagates edited truss type dimensions in the table before scene syncing.
void PropagateSharedTrussTypeDimensionValues(wxDataViewListCtrl *table,
                                             const wxDataViewItemArray &selections,
                                             TrussColumn column) {
    if (!table || !IsSharedTrussTypeDimensionColumn(column))
        return;

    const int col = ColumnIndex(column);
    std::unordered_map<std::string, wxVariant> valuesByType;
    for (const auto &item : selections) {
        const int row = table->ItemToRow(item);
        if (row == wxNOT_FOUND)
            continue;
        wxVariant value;
        table->GetValue(value, row, col);
        valuesByType[BuildVisibleTrussTypeKey(table, row)] = value;
    }

    const unsigned int rowCount = table->GetItemCount();
    for (unsigned int row = 0; row < rowCount; ++row) {
        auto it = valuesByType.find(BuildVisibleTrussTypeKey(table, row));
        if (it == valuesByType.end())
            continue;
        wxVariant current;
        table->GetValue(current, row, col);
        if (current.GetString() == it->second.GetString())
            continue;
        table->SetValue(it->second, row, col);
    }
}

const wxString &DegreeSymbol() {
  static const wxString kDegreeSymbol = wxString::FromUTF8("\xC2\xB0");
  return kDegreeSymbol;
}

constexpr const char *kUnassignedPosition = "Unassigned";

std::string NormalizePositionName(const std::string &positionName) {
    return positionName.empty() ? kUnassignedPosition : positionName;
}

Units::DistanceUnitSystem ResolveDistanceUnitSystem() {
    auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return Units::ParseDistanceUnitSystem(
      cfg.GetValue("ui_distance_unit_system"));
}

Units::WeightUnitSystem ResolveWeightUnitSystem() {
    auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    return Units::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
}

struct RangeParts {
    wxArrayString parts;
    bool usedSeparator = false;
    bool trailingSeparator = false;
};

void AppendTrussUpdateLog(
    const std::vector<std::pair<std::string, std::string>> &updates,
    bool logChanges) {
    if (!logChanges)
        return;

    ConsolePanel* console = ConsolePanel::Instance();
    if (!console || updates.empty())
        return;

    if (updates.size() == 1)
    {
        const auto& [name, uuid] = updates.front();
        console->AppendMessage("Updated truss " + wxString::FromUTF8(name) +
                               " (UUID " + wxString::FromUTF8(uuid) + ")");
        return;
    }

    console->AppendMessage(wxString::Format("Updated %zu trusses", updates.size()));
}

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

TrussTablePanel::TrussTablePanel(wxWindow* parent, IGuiConfigServices* services)
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
  table->Bind(wxEVT_LEFT_DOWN, &TrussTablePanel::OnLeftDown, this);
    table->Bind(wxEVT_LEFT_UP, &TrussTablePanel::OnLeftUp, this);
    table->Bind(wxEVT_MOTION, &TrussTablePanel::OnMouseMove, this);
    table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
                &TrussTablePanel::OnSelectionChanged, this);
  table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
              &TrussTablePanel::OnContextMenu, this);

  table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
              &TrussTablePanel::OnItemActivated, this);
  table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED, &TrussTablePanel::OnColumnSorted,
              this);

    Bind(wxEVT_MOUSE_CAPTURE_LOST, &TrussTablePanel::OnCaptureLost, this);

    deferredSelectionGuard = std::make_unique<gui::DataViewDeferredSelectionGuard>(
            this, table,
            [this](const wxDataViewItem &item) { return UuidForItem(item); },
            [this](const std::string &uuid) {
                auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
                if (pos == rowUuids.end())
                    return wxDataViewItem();
                return table->RowToItem(static_cast<unsigned int>(pos - rowUuids.begin()));
            },
            [this]() { SyncSelectionFromTable(); },
            [this]() { UpdateSelectionHighlight(); });

    InitializeTable();
    ReloadData();

    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

// Releases table resources and detaches the truss pane from AUI layout management.
TrussTablePanel::~TrussTablePanel()
{
    if (wxAuiManager *manager = wxAuiManager::GetManager(this))
        manager->DetachPane(this);
    store = nullptr;
}

void TrussTablePanel::InitializeTable()
{
    const auto distanceUnit = ResolveDistanceUnitSystem();
    const auto weightUnit = ResolveWeightUnitSystem();
    const wxString distanceSuffix = wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnit));
    const wxString weightSuffix = wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
    columnLabels = {_("Name"), _("Layer"), _("Model File"), _("Hang Pos"),
                    ui::LocalizedLabelWithUnit(_("Pos X"), distanceSuffix), ui::LocalizedLabelWithUnit(_("Pos Y"), distanceSuffix), ui::LocalizedLabelWithUnit(_("Pos Z"), distanceSuffix),
                    "Roll (X)", "Pitch (Y)", "Yaw (Z)",
                    _("Manufacturer"), _("Model"),
                    ui::LocalizedLabelWithUnit(_("Length"), distanceSuffix), ui::LocalizedLabelWithUnit(_("Width"), distanceSuffix), ui::LocalizedLabelWithUnit(_("Height"), distanceSuffix), ui::LocalizedLabelWithUnit(_("Weight"), weightSuffix), ui::LocalizedLabelWithUnit(_("Load"), weightSuffix)};
    std::vector<int> widths = {150, 100, 180, 120,
                               80, 80, 80,
                               80, 80, 80,
                               120, 120,
                               90, 90, 90, 90, 90};
    if (columnLabels.size() != TableColumnIndices::Count<TrussColumn>() ||
        widths.size() != TableColumnIndices::Count<TrussColumn>())
        return;
    for (size_t i = 0; i < columnLabels.size(); ++i)
        table->AppendColumn(new wxDataViewColumn(
            columnLabels[i], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                      wxALIGN_LEFT),
            i, widths[i], wxALIGN_LEFT,
            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
    ColumnUtils::EnforceMinColumnWidth(table);
}

// Refreshes truss table rows from the current project data.
void TrussTablePanel::ReloadData() {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContentChanged();
    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
    const std::vector<std::string> selectedTrusses = cfg.GetSelectedTrusses();
    const auto distanceUnit = ResolveDistanceUnitSystem();
    const auto weightUnit = ResolveWeightUnitSystem();
    const wxString distanceSuffix =
        wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnit));
    const wxString weightSuffix =
        wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
  columnLabels[ColumnIndex(TrussColumn::PositionX)] = ui::LocalizedLabelWithUnit(_("Pos X"), distanceSuffix);
  columnLabels[ColumnIndex(TrussColumn::PositionY)] = ui::LocalizedLabelWithUnit(_("Pos Y"), distanceSuffix);
  columnLabels[ColumnIndex(TrussColumn::PositionZ)] = ui::LocalizedLabelWithUnit(_("Pos Z"), distanceSuffix);
  columnLabels[ColumnIndex(TrussColumn::Length)] = ui::LocalizedLabelWithUnit(_("Length"), distanceSuffix);
  columnLabels[ColumnIndex(TrussColumn::Width)] = ui::LocalizedLabelWithUnit(_("Width"), distanceSuffix);
  columnLabels[ColumnIndex(TrussColumn::Height)] = ui::LocalizedLabelWithUnit(_("Height"), distanceSuffix);
  columnLabels[ColumnIndex(TrussColumn::Weight)] = ui::LocalizedLabelWithUnit(_("Weight"), weightSuffix);
    for (size_t i = 0; i < columnLabels.size(); ++i) {
        if (auto *column = table->GetColumn(static_cast<unsigned int>(i)))
            column->SetTitle(columnLabels[i]);
    }

  std::unique_ptr<wxEventBlocker> selectionBlocker =
      std::make_unique<wxEventBlocker>(table, wxEVT_DATAVIEW_SELECTION_CHANGED);

    table->DeleteAllItems();
    rowUuids.clear();
    modelPaths.clear();
    symbolPaths.clear();
    rowUuidByKey.clear();
    modelPathByKey.clear();
    symbolPathByKey.clear();
    nextRowKey = 1;
    const auto& trusses = cfg.GetScene().trusses;

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
        const std::string &base = cfg.GetScene().basePath;
        const std::string modelReference =
            !truss.gdtfSpec.empty() ? truss.gdtfSpec : truss.modelFile;
        if (!modelReference.empty()) {
            fs::path p = base.empty() ? fs::path(modelReference)
                                     : fs::path(base) / modelReference;
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
        wxString posX = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
            posArr[0], distanceUnit, Units::ValueFormatContext::Table));
        wxString posY = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
            posArr[1], distanceUnit, Units::ValueFormatContext::Table));
        wxString posZ = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
            posArr[2], distanceUnit, Units::ValueFormatContext::Table));

        auto euler = MatrixUtils::MatrixToEuler(truss.transform);
        wxString roll = wxString::Format("%.1f", euler[2]) + DegreeSymbol();
        wxString pitch = wxString::Format("%.1f", euler[1]) + DegreeSymbol();
        wxString yaw = wxString::Format("%.1f", euler[0]) + DegreeSymbol();

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
        wxString len = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
            truss.lengthMm, distanceUnit, Units::ValueFormatContext::Table));
    wxString wid =
        truss.widthMm > 0.0f
                           ? wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
                                 truss.widthMm, distanceUnit,
                                 Units::ValueFormatContext::Table))
                           : wxString();
    wxString hei =
        truss.heightMm > 0.0f
                            ? wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
                                  truss.heightMm, distanceUnit,
                                  Units::ValueFormatContext::Table))
                            : wxString();
        wxString weight = wxString::FromUTF8(Units::FormatWeightFromKilograms(
            truss.weightKg, weightUnit, Units::ValueFormatContext::Table));
        wxString load;
        if (truss.hasManualLoadOverride) {
            load = wxString::FromUTF8(Units::FormatWeightFromKilograms(
                truss.manualLoadKg, weightUnit, Units::ValueFormatContext::Table));
        }
        row.push_back(manuf);
        row.push_back(modelName);
        row.push_back(len);
        row.push_back(wid);
        row.push_back(hei);
        row.push_back(weight);
        row.push_back(load);

        const wxUIntPtr rowKey = nextRowKey++;
        store->AppendItem(row, rowKey);
        if (truss.hasManualLoadOverride)
            store->SetCellTextColour(store->GetCount() - 1,
                                     ColumnIndex(TrussColumn::Load), *wxRED);
        rowUuids.push_back(uuid);
        rowUuidByKey[rowKey] = uuid;
        modelPathByKey[rowKey] = modelFull;
        symbolPathByKey[rowKey] = wxString::FromUTF8(symbolFullPath);
    }

    selectionBlocker.reset();
    if (!selectedTrusses.empty())
        SelectByUuid(selectedTrusses, false);
    UpdateSelectionHighlight();

    // Let wxDataViewListCtrl manage column headers and sorting
    if (LayerPanel::Instance())
        LayerPanel::Instance()->ReloadLayers();
    if (SummaryPanel::Instance() && IsActivePage())
        SummaryPanel::Instance()->ShowTrussSummary();
}

// Handles context-menu editing workflows and avoids expensive refreshes when no
// row values change.
void TrussTablePanel::OnContextMenu(wxDataViewEvent &event) {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyItemActivated(event.GetItem(), event.GetColumn());
    wxDataViewItem item = event.GetItem();
    int col = event.GetColumn();
  const auto namedColumn = TableColumnIndices::FromIndex<TrussColumn>(col);
  if (!item.IsOk() || !namedColumn ||
      static_cast<size_t>(col) >= columnLabels.size())
        return;

    // Freeze UI updates while performing bulk table modifications to avoid
    // excessive repaints. This RAII locker calls Freeze() on the table and
    // Thaw() automatically when it is destroyed at the end of the function,
    // improving responsiveness during edits.
    wxWindowUpdateLocker locker(table);

    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        selections.push_back(item);

    std::vector<std::string> selectedUuids;
  for (const auto &it : selections) {
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

    // Layer column uses a dropdown of existing layers
  if (*namedColumn == TrussColumn::Layer) {
        auto layers = guiConfigServices->LegacyConfigManager().GetLayerNames();
        wxArrayString choices;
        for (const auto& n : layers)
            choices.push_back(wxString::FromUTF8(n));
        wxSingleChoiceDialog sdlg(this, "Select layer", "Layer", choices);
        if (sdlg.ShowModal() != wxID_OK)
            return;
        wxString sel = sdlg.GetStringSelection();
    wxString val =
        sel == wxString::FromUTF8(DEFAULT_LAYER_NAME) ? wxString() : sel;
    for (const auto &itSel : selections) {
            int r = table->ItemToRow(itSel);
            if (r != wxNOT_FOUND)
                table->SetValue(wxVariant(val), r, col);
        }
        ResyncRows(oldOrder, selectedUuids);
        UpdateSceneData();
    if (Viewer3DPanel::Instance()) {
            Viewer3DPanel::Instance()->UpdateScene();
            Viewer3DPanel::Instance()->Refresh();
    } else if (Viewer2DPanel::Instance()) {
            Viewer2DPanel::Instance()->UpdateScene();
        }
        return;
    }

    // Model File column opens file dialog
  if (*namedColumn == TrussColumn::ModelFile) {
        wxString trussDir =
            wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
        wxFileDialog fdlg(this, "Select Truss Model", trussDir, wxEmptyString,
                      "Truss files "
                      "(*.gdtf;*.gtruss;*.3ds;*.glb)|*.gdtf;*.gtruss;*.3ds;*."
                      "glb|All files|*.*",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() == wxID_OK) {
            bool changed = false;
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
        table->GetValue(mv, row, ColumnIndex(TrussColumn::Model));
                modelNameWx = mv.GetString();
                modelKey = std::string(modelNameWx.ToUTF8());
            }

      if (LoadTrussDefinition(archivePath, parsed)) {
                if (!parsed.symbolFile.empty())
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
      for (const auto &itSel : selections) {
                int r = table->ItemToRow(itSel);
                if (r == wxNOT_FOUND)
                    continue;
                const wxString archivePathWx = wxString::FromUTF8(archivePath);
                const wxString geomPathWx = wxString::FromUTF8(geomPath);
                const bool pathChanged =
                    static_cast<size_t>(r) >= modelPaths.size() ||
                    static_cast<size_t>(r) >= symbolPaths.size() ||
                    modelPaths[static_cast<size_t>(r)] != archivePathWx ||
                    symbolPaths[static_cast<size_t>(r)] != geomPathWx;

        SetModelPathsForRow(static_cast<unsigned int>(r), archivePathWx,
                            geomPathWx);
                wxVariant existingModelFile;
        table->GetValue(existingModelFile, r,
                        ColumnIndex(TrussColumn::ModelFile));
        if (existingModelFile.GetString() != fileName) {
          table->SetValue(wxVariant(fileName), r,
                          ColumnIndex(TrussColumn::ModelFile));
                    changed = true;
                }
                changed = changed || pathChanged;
        if (parsedOk) {
          table->SetValue(wxVariant(manuf), r,
                          ColumnIndex(TrussColumn::Manufacturer));
          table->SetValue(wxVariant(modelNameWx), r,
                          ColumnIndex(TrussColumn::Model));
          table->SetValue(wxVariant(lenStr), r,
                          ColumnIndex(TrussColumn::Length));
          table->SetValue(wxVariant(widStr), r,
                          ColumnIndex(TrussColumn::Width));
          table->SetValue(wxVariant(heiStr), r,
                          ColumnIndex(TrussColumn::Height));
          table->SetValue(wxVariant(weightStr), r,
                          ColumnIndex(TrussColumn::Weight));
                }
            }
            if (!changed)
                return;
            TrussDictionary::Update(modelKey, archivePath);
            ResyncRows(oldOrder, selectedUuids);
            UpdateSceneData();
      if (Viewer3DPanel::Instance()) {
                Viewer3DPanel::Instance()->UpdateScene();
                Viewer3DPanel::Instance()->Refresh();
      } else if (Viewer2DPanel::Instance()) {
                Viewer2DPanel::Instance()->UpdateScene();
            }
        }
        return;
    }

  if (*namedColumn == TrussColumn::HangPosition) {
    std::vector<unsigned int> selectedRows;
    for (const auto &it : selections) {
      const int row = table->ItemToRow(it);
      if (row != wxNOT_FOUND)
        selectedRows.push_back(static_cast<unsigned int>(row));
    }

    HangPositionDialogResult result;
    if (!ShowHangPositionDialog(this, current.GetString(), &result))
      return;
    ApplySharedHangPositionChanges(result, false, {}, true, selectedRows, false,
                                    {});
    ResyncRows(oldOrder, selectedUuids);
    return;
  }

  wxTextEntryDialog dlg(this, _("Edit value:"), columnLabels[col],
                        current.GetString());
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString value = dlg.GetValue().Trim(true).Trim(false);

  const bool numericCol = IsTransformColumn(*namedColumn);
    bool relative = false;
    double delta = 0.0;
  if (numericCol && (value.StartsWith("++") || value.StartsWith("--"))) {
        wxString numStr = value.Mid(2);
    if (numStr.ToDouble(&delta)) {
            if (value.StartsWith("--"))
                delta = -delta;
            relative = true;
        }
    }

  if (numericCol) {
    if (relative) {
      for (const auto &it : selections) {
                int r = table->ItemToRow(it);
                if (r == wxNOT_FOUND)
                    continue;
                wxVariant cv;
                table->GetValue(cv, r, col);
                wxString cur = cv.GetString();
        if (IsRotationColumn(*namedColumn)) {
                    if (!DegreeSymbol().empty())
                        cur.Replace(DegreeSymbol(), "");
                }
                double curVal = 0.0;
                cur.ToDouble(&curVal);
                double newVal = curVal + delta;
                wxString out;
        if (IsRotationColumn(*namedColumn))
                    out = wxString::Format("%.1f", newVal) + DegreeSymbol();
                else
                    out = wxString::Format("%.3f", newVal);
                table->SetValue(wxVariant(out), r, col);
            }
    } else {
            RangeParts range = SplitRangeParts(value);
            wxArrayString parts = range.parts;
      if (parts.size() == 0 || parts.size() > 2) {
                wxMessageBox(_("Invalid numeric value"), _("Error"), wxOK | wxICON_ERROR);
                return;
            }
            if (range.usedSeparator && parts.size() != 2 &&
          !(parts.size() == 1 && range.trailingSeparator)) {
                wxMessageBox(_("Invalid numeric value"), _("Error"), wxOK | wxICON_ERROR);
                return;
            }

            double v1, v2 = 0.0;
      if (!parts[0].ToDouble(&v1)) {
                wxMessageBox(_("Invalid value"), _("Error"), wxOK | wxICON_ERROR);
                return;
            }
            bool interp = false;
            bool sequential = false;
      if (parts.size() == 2) {
        if (!parts[1].ToDouble(&v2)) {
                    wxMessageBox(_("Invalid value"), _("Error"), wxOK | wxICON_ERROR);
                    return;
                }
                interp = selections.size() > 1;
      } else if (range.usedSeparator && range.trailingSeparator) {
                sequential = selections.size() > 1;
            }

      for (size_t i = 0; i < selections.size(); ++i) {
                double val = v1;
                if (interp)
                    val = v1 + (v2 - v1) * i / (selections.size() - 1);
                else if (sequential)
                    val = v1 + static_cast<double>(i);

                wxString out;
        if (IsRotationColumn(*namedColumn))
                    out = wxString::Format("%.1f", val) + DegreeSymbol();
                else
                    out = wxString::Format("%.3f", val);

                int r = table->ItemToRow(selections[i]);
                if (r != wxNOT_FOUND)
                    table->SetValue(wxVariant(out), r, col);
            }
        }
  } else {
    for (const auto &it : selections) {
            int r = table->ItemToRow(it);
            if (r != wxNOT_FOUND)
                table->SetValue(wxVariant(value), r, col);
        }
    }

    PropagateSharedTrussTypeDimensionValues(table, selections, *namedColumn);

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
        CaptureMouse();
    }
    evt.Skip();
}

void TrussTablePanel::OnLeftUp(wxMouseEvent &evt) {
  if (dragSelecting) {
        dragSelecting = false;
        ReleaseMouse();
    }
    evt.Skip();
}

void TrussTablePanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(evt)) {
    dragSelecting = false;
}

void TrussTablePanel::OnMouseMove(wxMouseEvent &evt) {
  if (!dragSelecting || !evt.Dragging()) {
        evt.Skip();
        return;
    }
    wxDataViewItem item;
    wxDataViewColumn* col;
    table->HitTest(evt.GetPosition(), item, col);
    int row = table->ItemToRow(item);
  if (row != wxNOT_FOUND) {
        if (deferredSelectionGuard)
            deferredSelectionGuard->NotifyDragStarted();
        int minRow = std::min(startRow, row);
        int maxRow = std::max(startRow, row);
        table->UnselectAll();
        for (int r = minRow; r <= maxRow; ++r)
            table->SelectRow(r);
    }
    evt.Skip();
}

// Handles table selection events and defers transient single-click collapses.
void TrussTablePanel::OnSelectionChanged(wxDataViewEvent &evt) {
  if (deferredSelectionGuard && deferredSelectionGuard->HandleSelectionChanged())
    return;

  SyncSelectionFromTable();
  evt.Skip();
}

// Synchronizes selected table rows with the shared scene selection state.
void TrussTablePanel::SyncSelectionFromTable() {
    RebuildRowCachesFromRowKeys();
    const selection::Origin origin = selection::CurrentOrigin();
    if (origin == selection::Origin::Viewer2D ||
        origin == selection::Origin::Viewer3D) {
        UpdateSelectionHighlight();
              return;
    }

    wxDataViewItemArray selections;
    table->GetSelections(selections);
    std::vector<std::string> uuids;
    uuids.reserve(selections.size());
  for (const auto &it : selections) {
        const std::string uuid = UuidForItem(it);
        if (!uuid.empty())
            uuids.push_back(uuid);
    }
    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
    if (uuids != cfg.GetSelectedTrusses()) {
        cfg.PushUndoState("truss selection");
        cfg.SetSelectedTrusses(uuids);
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

}

void TrussTablePanel::UpdateSelectionHighlight() {
    size_t rowCount = table->GetItemCount();
    std::vector<bool> selectedRows(rowCount, false);
    wxDataViewItemArray selections;
    table->GetSelections(selections);
  for (const auto &it : selections) {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND && static_cast<size_t>(r) < rowCount)
            selectedRows[r] = true;
    }
    store->SetSelectedRows(selectedRows);
}

void TrussTablePanel::UpdatePositionValues(
    const std::vector<std::string>& uuids) {
    if (!table)
        return;

    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
    auto& scene = cfg.GetScene();
    wxWindowUpdateLocker locker(table);

    for (const auto& uuid : uuids) {
        auto it = scene.trusses.find(uuid);
        if (it == scene.trusses.end())
            continue;

        auto posArr = it->second.transform.o;
        wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
        wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
        wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);

        auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
        if (pos == rowUuids.end())
            continue;

        int row = static_cast<int>(pos - rowUuids.begin());
    table->SetValue(wxVariant(posX), row, ColumnIndex(TrussColumn::PositionX));
    table->SetValue(wxVariant(posY), row, ColumnIndex(TrussColumn::PositionY));
    table->SetValue(wxVariant(posZ), row, ColumnIndex(TrussColumn::PositionZ));
    }
}

void TrussTablePanel::ApplyPositionValueUpdates(
    const std::vector<PositionValueUpdate>& updates) {
    if (!table)
        return;

    wxWindowUpdateLocker locker(table);
    for (const auto& update : updates) {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), update.uuid);
        if (pos == rowUuids.end())
            continue;

        int row = static_cast<int>(pos - rowUuids.begin());
        table->SetValue(wxVariant(wxString::FromUTF8(update.posX)), row,
                        ColumnIndex(TrussColumn::PositionX));
        table->SetValue(wxVariant(wxString::FromUTF8(update.posY)), row,
                        ColumnIndex(TrussColumn::PositionY));
        table->SetValue(wxVariant(wxString::FromUTF8(update.posZ)), row,
                        ColumnIndex(TrussColumn::PositionZ));
    }
}

// Opens the truss edit dialog for the activated table row.
void TrussTablePanel::OnItemActivated(wxDataViewEvent &event) {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContextActionStarted();
  const wxDataViewItem item =
      event.GetItem().IsOk() ? event.GetItem() : table->GetSelection();
  if (!item.IsOk())
    return;
  const int row = table->ItemToRow(item);
  if (row < 0 || static_cast<size_t>(row) >= rowUuids.size())
    return;

  TrussEditDialog dialog(this, row);
  dialog.ShowModal();
}

// Applies edited truss table values back into the scene data model.
void TrussTablePanel::UpdateSceneData(bool logChanges)
{
    // Ensure in-place cell editors commit pending values before reading table rows.
    if (table)
        DataViewEditCommit::CommitPendingEdit(table);


    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
    auto& scene = cfg.GetScene();
    size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());

    struct Dim {
        float len;
        float wid;
        float hei;
        float weight;
    };
    std::unordered_map<std::string, Dim> dims;
    std::unordered_set<std::string> changedTrussIds;
    std::unordered_set<std::string> changedWeightPositions;
    std::vector<std::pair<std::string, std::string>> updatedTrusses;

  auto makeKey = [](const std::string &n, const std::string &m,
                    const std::string &mo) { return n + "" + m + "" + mo; };

    bool undoPushed = false;
    bool anyChanged = false;
    auto pushUndoIfNeeded = [&]() {
    if (!undoPushed) {
            cfg.PushUndoState("edit truss");
            undoPushed = true;
        }
    };

  // First pass: compute row updates and track canonical dimensions per truss
  // group.
  for (size_t i = 0; i < count; ++i) {
        auto it = scene.trusses.find(rowUuids[i]);
        if (it == scene.trusses.end())
            continue;

        const Truss old = it->second;
        Truss next = old;
        wxVariant v;

    table->GetValue(v, i, ColumnIndex(TrussColumn::Name));
        next.name = std::string(v.GetString().mb_str());

    table->GetValue(v, i, ColumnIndex(TrussColumn::Layer));
        std::string layerStr = std::string(v.GetString().mb_str());
        if (layerStr.empty())
            next.layer.clear();
        else
            next.layer = layerStr;

        if (i < symbolPaths.size())
            next.symbolFile = gui::PreserveSceneResourceReferenceForTableSync(
                scene.basePath, old.symbolFile, std::string(symbolPaths[i].ToUTF8()));
        else if (i < modelPaths.size())
            next.symbolFile = gui::PreserveSceneResourceReferenceForTableSync(
                scene.basePath, old.symbolFile, std::string(modelPaths[i].ToUTF8()));
        else {
      table->GetValue(v, i, ColumnIndex(TrussColumn::ModelFile));
            next.symbolFile = std::string(v.GetString().ToUTF8());
        }

        if (i < modelPaths.size())
            next.modelFile = gui::PreserveSceneResourceReferenceForTableSync(
                scene.basePath, old.modelFile, std::string(modelPaths[i].ToUTF8()),
                old.symbolFile);
        else {
      table->GetValue(v, i, ColumnIndex(TrussColumn::ModelFile));
            next.modelFile = std::string(v.GetString().ToUTF8());
        }

    table->GetValue(v, i, ColumnIndex(TrussColumn::HangPosition));
        next.positionName = std::string(v.GetString().mb_str());

        const auto distanceUnit = ResolveDistanceUnitSystem();
        const auto weightUnit = ResolveWeightUnitSystem();
    double xMm = old.transform.o[0], yMm = old.transform.o[1],
           zMm = old.transform.o[2];
    table->GetValue(v, i, ColumnIndex(TrussColumn::PositionX));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnit);
        parsed.has_value())
            xMm = *parsed;
    table->GetValue(v, i, ColumnIndex(TrussColumn::PositionY));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnit);
        parsed.has_value())
            yMm = *parsed;
    table->GetValue(v, i, ColumnIndex(TrussColumn::PositionZ));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnit);
        parsed.has_value())
            zMm = *parsed;

        double roll = 0, pitch = 0, yaw = 0;
    table->GetValue(v, i, ColumnIndex(TrussColumn::Roll));
        {
            wxString s = v.GetString();
            if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
            s.ToDouble(&roll);
        }
    table->GetValue(v, i, ColumnIndex(TrussColumn::Pitch));
        {
            wxString s = v.GetString();
            if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
            s.ToDouble(&pitch);
        }
    table->GetValue(v, i, ColumnIndex(TrussColumn::Yaw));
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

    if (transformChanged) {
            Matrix rot = MatrixUtils::EulerToMatrix(static_cast<float>(yaw),
                                                    static_cast<float>(pitch),
                                                    static_cast<float>(roll));
            next.transform = MatrixUtils::ApplyRotationPreservingScale(
                old.transform, rot,
          {static_cast<float>(xMm), static_cast<float>(yMm),
                 static_cast<float>(zMm)});
        }

    table->GetValue(v, i, ColumnIndex(TrussColumn::Manufacturer));
        next.manufacturer = std::string(v.GetString().mb_str());
    table->GetValue(v, i, ColumnIndex(TrussColumn::Model));
        next.model = std::string(v.GetString().mb_str());

    table->GetValue(v, i, ColumnIndex(TrussColumn::Length));
        if (const auto parsed = Units::ParseDistanceToMillimeters(
                std::string(v.GetString().ToUTF8()), distanceUnit);
            parsed.has_value())
            next.lengthMm = static_cast<float>(*parsed);
    table->GetValue(v, i, ColumnIndex(TrussColumn::Width));
        if (const auto parsed = Units::ParseDistanceToMillimeters(
                std::string(v.GetString().ToUTF8()), distanceUnit);
            parsed.has_value())
            next.widthMm = static_cast<float>(*parsed);
    table->GetValue(v, i, ColumnIndex(TrussColumn::Height));
        if (const auto parsed = Units::ParseDistanceToMillimeters(
                std::string(v.GetString().ToUTF8()), distanceUnit);
            parsed.has_value())
            next.heightMm = static_cast<float>(*parsed);
    table->GetValue(v, i, ColumnIndex(TrussColumn::Weight));
        if (const auto parsed = Units::ParseWeightToKilograms(
                std::string(v.GetString().ToUTF8()), weightUnit);
            parsed.has_value())
            next.weightKg = static_cast<float>(*parsed);
    table->GetValue(v, i, ColumnIndex(TrussColumn::Load));
        {
            const std::string loadText = std::string(v.GetString().ToUTF8());
            if (loadText.empty()) {
                next.manualLoadKg = 0.0f;
                next.hasManualLoadOverride = false;
            } else if (const auto parsed = Units::ParseWeightToKilograms(
                           loadText, weightUnit);
                       parsed.has_value()) {
                next.manualLoadKg = static_cast<float>(*parsed);
                next.hasManualLoadOverride = true;
            }
        }

        const bool trussChanged =
        old.name != next.name || old.layer != next.layer ||
        old.modelFile != next.modelFile || old.symbolFile != next.symbolFile ||
        old.positionName != next.positionName || transformChanged ||
        old.manufacturer != next.manufacturer || old.model != next.model ||
        !Units::NearlyEqualDistanceMillimeters(old.lengthMm, next.lengthMm,
                                               0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.widthMm, next.widthMm,
                                               0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.heightMm, next.heightMm,
                                               0.5) ||
            !Units::NearlyEqualWeightKilograms(old.weightKg, next.weightKg, 0.001) ||
        old.hasManualLoadOverride != next.hasManualLoadOverride ||
        !Units::NearlyEqualWeightKilograms(old.manualLoadKg, next.manualLoadKg,
                                           0.001);
        const bool weightChanged =
            !Units::NearlyEqualWeightKilograms(old.weightKg, next.weightKg, 0.001);
        const bool hangPositionChanged = old.positionName != next.positionName;

    if (trussChanged) {
            pushUndoIfNeeded();
            anyChanged = true;
            if (weightChanged || hangPositionChanged) {
                changedWeightPositions.insert(NormalizePositionName(old.positionName));
                changedWeightPositions.insert(NormalizePositionName(next.positionName));
            }
            it->second = next;
            if (!it->second.position.empty())
                scene.positions[it->second.position] = it->second.positionName;
            changedTrussIds.insert(it->second.uuid);
            updatedTrusses.emplace_back(it->second.name, it->second.uuid);
        }

        const Truss& canonicalSource = trussChanged ? it->second : old;
        std::string key = makeKey(canonicalSource.name,
                                  canonicalSource.manufacturer,
                                  canonicalSource.model);

        if (trussChanged || !dims.count(key))
        {
            dims[key] = {canonicalSource.lengthMm, canonicalSource.widthMm,
                         canonicalSource.heightMm, canonicalSource.weightKg};
        }
    }

    // Second pass: synchronize dimensions across equal truss type groups.
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

        const float lenMm = dit->second.len;
        const float widMm = dit->second.wid;
        const float heiMm = dit->second.hei;
        const float weightKg = dit->second.weight;

        const bool synchronizedWeightChanged =
            !Units::NearlyEqualWeightKilograms(it->second.weightKg, weightKg, 0.001);
        if (it->second.lengthMm != lenMm || it->second.widthMm != widMm ||
        it->second.heightMm != heiMm || synchronizedWeightChanged) {
            pushUndoIfNeeded();
            anyChanged = true;
            it->second.lengthMm = lenMm;
            it->second.widthMm = widMm;
            it->second.heightMm = heiMm;
            it->second.weightKg = weightKg;
            if (synchronizedWeightChanged) {
      changedWeightPositions.insert(
          NormalizePositionName(it->second.positionName));
            }

            wxString lenStr = wxString::Format("%.2f", lenMm / 1000.0f);
            wxString widStr =
                widMm > 0.0f ? wxString::Format("%.2f", widMm / 1000.0f) : wxString();
            wxString heiStr =
                heiMm > 0.0f ? wxString::Format("%.2f", heiMm / 1000.0f) : wxString();
            wxString weiStr = wxString::Format("%.2f", weightKg);
      table->SetValue(wxVariant(lenStr), i, ColumnIndex(TrussColumn::Length));
      table->SetValue(wxVariant(widStr), i, ColumnIndex(TrussColumn::Width));
      table->SetValue(wxVariant(heiStr), i, ColumnIndex(TrussColumn::Height));
      table->SetValue(wxVariant(weiStr), i, ColumnIndex(TrussColumn::Weight));

            if (changedTrussIds.insert(it->second.uuid).second)
                updatedTrusses.emplace_back(it->second.name, it->second.uuid);
        }
    }

    AppendTrussUpdateLog(updatedTrusses, logChanges);

    if (!anyChanged)
        return;

  HoistLoadRecalculationPrompt::PromptAndApply(cfg, this,
                                               changedWeightPositions);

    if (SummaryPanel::Instance() && IsActivePage())
        SummaryPanel::Instance()->ShowTrussSummary();

    if (RiggingPanel::Instance())
        RiggingPanel::Instance()->RefreshData();

    gui::sceneviewrefresh::RefreshSceneViewsAfterTableEdit(
        this, gui::sceneviewrefresh::SceneUpdateScope::Full);
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

// Applies a primary hover highlight to one truss row.
void TrussTablePanel::HighlightTruss(const std::string& uuid)
{
    HighlightTruss(uuid, {});
}

// Applies primary and related group-hover highlights to truss rows.
void TrussTablePanel::HighlightTruss(
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

void TrussTablePanel::ClearSelection() {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContentChanged();
    table->UnselectAll();
    UpdateSelectionHighlight();
}

std::vector<std::string> TrussTablePanel::GetSelectedUuids() const {
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

void TrussTablePanel::SelectByUuid(const std::vector<std::string>& uuids,
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

void TrussTablePanel::DeleteSelected(bool pushUndoState) {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContentChanged();
    RebuildRowCachesFromRowKeys();
    wxDataViewItemArray selections;
    table->GetSelections(selections);
    if (selections.empty())
        return;

    ConfigManager& cfg = guiConfigServices->LegacyConfigManager();
    if (pushUndoState)
        cfg.PushUndoState("delete truss");
    cfg.SetSelectedTrusses({});

    std::vector<int> rows;
    rows.reserve(selections.size());
    for (const auto& it : selections) {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND)
            rows.push_back(r);
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    const std::vector<std::string> oldOrder = rowUuids;
    const std::vector<wxString> oldModelPaths = modelPaths;
    const std::vector<wxString> oldSymbolPaths = symbolPaths;

    auto& scene = guiConfigServices->LegacyConfigManager().GetScene();
    for (int r : rows) {
        if ((size_t)r < rowUuids.size()) {
            wxDataViewItem rowItem = table->RowToItem(static_cast<unsigned int>(r));
            const wxUIntPtr rowKey = store->GetItemData(rowItem);
            scene.trusses.erase(rowUuids[r]);
            rowUuidByKey.erase(rowKey);
            modelPathByKey.erase(rowKey);
            symbolPathByKey.erase(rowKey);
            table->DeleteItem(r);
        }
    }

    rowUuids = oldOrder;
    modelPaths = oldModelPaths;
    symbolPaths = oldSymbolPaths;

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
  } else if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->SetSelectedUuids(mergedSelection);
        Viewer2DPanel::Instance()->UpdateScene();
    }

    if (SummaryPanel::Instance())
        SummaryPanel::Instance()->ShowTrussSummary();

    if (RiggingPanel::Instance())
        RiggingPanel::Instance()->RefreshData();

    ResyncRows(oldOrder, {});
}

void TrussTablePanel::ResyncRows(
    const std::vector<std::string> &oldOrder,
    const std::vector<std::string> &selectedUuids) {
    (void)oldOrder;
    RebuildRowCachesFromRowKeys();

    table->UnselectAll();
  for (const auto &uuid : selectedUuids) {
        auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
        if (pos != rowUuids.end())
            table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
    }
    UpdateSelectionHighlight();
}

void TrussTablePanel::RebuildRowCachesFromRowKeys() {
    if (!table || !store)
        return;
    const unsigned int count = table->GetItemCount();
    rowUuids.assign(count, std::string());
    modelPaths.assign(count, wxString());
    symbolPaths.assign(count, wxString());
    for (unsigned int row = 0; row < count; ++row) {
        wxDataViewItem item = table->RowToItem(row);
        const wxUIntPtr rowKey = store->GetItemData(item);
        auto uuidIt = rowUuidByKey.find(rowKey);
        if (uuidIt != rowUuidByKey.end())
            rowUuids[row] = uuidIt->second;
        auto modelIt = modelPathByKey.find(rowKey);
        if (modelIt != modelPathByKey.end())
            modelPaths[row] = modelIt->second;
        auto symbolIt = symbolPathByKey.find(rowKey);
        if (symbolIt != symbolPathByKey.end())
            symbolPaths[row] = symbolIt->second;
    }
}

std::string TrussTablePanel::UuidForItem(const wxDataViewItem& item) const {
    if (!store || !item.IsOk())
        return {};
    const wxUIntPtr rowKey = store->GetItemData(item);
    auto it = rowUuidByKey.find(rowKey);
    if (it == rowUuidByKey.end())
        return {};
    return it->second;
}

void TrussTablePanel::SetModelPathsForRow(unsigned int row,
                                          const wxString& modelPath,
                                          const wxString& symbolPath) {
    if (!table || !store || row >= table->GetItemCount())
        return;
    if (row >= modelPaths.size())
        modelPaths.resize(table->GetItemCount());
    if (row >= symbolPaths.size())
        symbolPaths.resize(table->GetItemCount());
    modelPaths[row] = modelPath;
    symbolPaths[row] = symbolPath;
    wxDataViewItem item = table->RowToItem(row);
    const wxUIntPtr rowKey = store->GetItemData(item);
    modelPathByKey[rowKey] = modelPath;
    symbolPathByKey[rowKey] = symbolPath;
}

// Reapplies UUID-based selection after user-driven column sorting changes row order.
void TrussTablePanel::OnColumnSorted(wxDataViewEvent &event) {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContentChanged();
    RebuildRowCachesFromRowKeys();
    const std::vector<std::string> selectedUuids =
        guiConfigServices->LegacyConfigManager().GetSelectedTrusses();
    std::vector<std::string> oldOrder = rowUuids;
    ResyncRows(oldOrder, selectedUuids);
    event.Skip();
}
