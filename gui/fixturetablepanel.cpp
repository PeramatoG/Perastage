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
#include "fixturetablepanel.h"
#include "addressdialog.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "fixtureeditdialog.h"
#include "fixturetable/fixture_table_columns.h"
#include "fixturetable/fixture_table_edit_service.h"
#include "fixturetable/fixture_table_parser.h"
#include "fixturetablepanel_ui_helpers.h"
#include "fixturetablepanel_update_types.h"
#include "gdtf_fixture_category.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "guiconfigservices.h"
#include "hoist_load_recalculation_prompt.h"
#include "layerpanel.h"
#include "mainwindow.h"
#include "matrixutils.h"
#include "patchmanager.h"
#include "projectutils.h"
#include "riggingpanel.h"
#include "selection_origin_token.h"
#include "stringutils.h"
#include "summarypanel.h"
#include "units/unit_label_utils.h"
#include "units/units.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <wx/aui/aui.h>
#include <wx/choicdlg.h>
#include <wx/colordlg.h>
#include <wx/dcmemory.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/tokenzr.h>
#include <wx/version.h>
#include <wx/wupdlock.h>

namespace fs = std::filesystem;

namespace {
// Returns the UTF-8 degree symbol used across fixture table labels.
const wxString &DegreeSymbol() {
  static const wxString kDegreeSymbol = wxString::FromUTF8("\xC2\xB0");
  return kDegreeSymbol;
}

class ConfigManagerSceneAdapter
    : public FixtureTableEditService::ISceneAdapter {
public:
  // Stores an undo checkpoint for fixture table edits.
  void PushUndoState(const std::string &description) override {
    GetDefaultGuiConfigServices().LegacyConfigManager().PushUndoState(
        description);
  }

  // Exposes the active scene for fixture table edit operations.
  MvrScene &GetScene() override {
    return GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  }

  // Reads the configured distance unit system for formatting and parsing.
  Units::DistanceUnitSystem GetDistanceUnitSystem() const override {
    const auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    return Units::ParseDistanceUnitSystem(
        cfg.GetValue("ui_distance_unit_system"));
  }

  // Reads the configured weight unit system for formatting and parsing.
  Units::WeightUnitSystem GetWeightUnitSystem() const override {
    const auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    return Units::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
  }
};

// Refreshes 2D/3D viewers after fixture data changes.
void RefreshViewersForFixtureUpdate(
    FixtureTablePanel::SceneDataUpdateType updateType) {
  const bool requiresFullSceneUpdate =
      FixtureTablePanel::RequiresFullViewerSceneUpdate(updateType);
  if (Viewer3DPanel::Instance()) {
    if (requiresFullSceneUpdate) {
      Viewer3DPanel::Instance()->UpdateScene();
    }
    Viewer3DPanel::Instance()->Refresh();
  } else if (Viewer2DPanel::Instance()) {
    if (requiresFullSceneUpdate)
      Viewer2DPanel::Instance()->UpdateScene();
    else
      Viewer2DPanel::Instance()->UpdateScene(false);
  }
}

// Determines whether a scene update type requires rigging panel refresh.
bool RequiresRiggingRefresh(FixtureTablePanel::SceneDataUpdateType updateType) {
  using SceneDataUpdateType = FixtureTablePanel::SceneDataUpdateType;
  switch (updateType) {
  case SceneDataUpdateType::kWeightOrPosition:
  case SceneDataUpdateType::kGeneral:
    return true;
  case SceneDataUpdateType::kVisualLabelOnly:
  case SceneDataUpdateType::kPatchOnly:
  case SceneDataUpdateType::kAppearanceOnly:
  case SceneDataUpdateType::kCategoryOnly:
  case SceneDataUpdateType::kTransformOnly:
  case SceneDataUpdateType::kMetadataOnly:
  case SceneDataUpdateType::kFixtureIdOnly:
    return false;
  }
  return true;
}

// Determines whether a scene update type requires summary panel refresh.
bool ShouldRefreshFixtureSummary(
    FixtureTablePanel::SceneDataUpdateType updateType) {
  using SceneDataUpdateType = FixtureTablePanel::SceneDataUpdateType;
  switch (updateType) {
  case SceneDataUpdateType::kGeneral:
  case SceneDataUpdateType::kCategoryOnly:
  case SceneDataUpdateType::kWeightOrPosition:
  case SceneDataUpdateType::kMetadataOnly:
    return true;
  case SceneDataUpdateType::kVisualLabelOnly:
  case SceneDataUpdateType::kPatchOnly:
  case SceneDataUpdateType::kAppearanceOnly:
  case SceneDataUpdateType::kTransformOnly:
  case SceneDataUpdateType::kFixtureIdOnly:
    return false;
  }
  return true;
}

// Checks whether a table cell is currently marked with a red validation color.
bool IsRedCell(const ColorfulDataViewListStore *store, int row, int col) {
  if (!store || row < 0 || col < 0)
    return false;

  const size_t rowIndex = static_cast<size_t>(row);
  const size_t colIndex = static_cast<size_t>(col);
  if (rowIndex >= store->cellAttrs.size())
    return false;
  if (colIndex >= store->cellAttrs[rowIndex].size())
    return false;

  const wxDataViewItemAttr &attr = store->cellAttrs[rowIndex][colIndex];
  return attr.HasColour() && attr.GetColour() == *wxRED;
}

// Renders and assigns the fixture color swatch cell for a given row.
void SetFixtureColorCell(wxDataViewListCtrl *table, int row,
                         const std::string &hexColor) {
  if (!table || row == wxNOT_FOUND || hexColor.empty())
    return;
  wxBitmap bmp(16, 16);
  wxMemoryDC dc(bmp);
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(wxColour(wxString::FromUTF8(hexColor))));
  dc.DrawRectangle(0, 0, 16, 16);
  dc.SelectObject(wxNullBitmap);

  wxVariant colorValue;
  colorValue << wxDataViewIconText(wxString::FromUTF8(hexColor), bmp);
  table->SetValue(
      colorValue, row,
      FixtureTableColumns::ToIndex(FixtureTableColumns::Column::VisualColor));
}
} // namespace

// Initializes the fixture table UI, columns, and event bindings.
FixtureTablePanel::FixtureTablePanel(wxWindow *parent,
                                     IGuiConfigServices *services)
    : wxPanel(parent, wxID_ANY),
      guiConfigServices(services ? services : &GetDefaultGuiConfigServices()) {
  store = new ColorfulDataViewListStore();
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxDV_MULTIPLE | wxDV_ROW_LINES);
  table->AssociateModel(store);
  store->DecRef();

  table->SetAlternateRowColour(wxColour(40, 40, 40));
  const wxColour selectionBackground(0, 255, 255);
  const wxColour selectionForeground(0, 0, 0);
  store->SetSelectionColours(selectionBackground, selectionForeground);
  BindTableHoverEvents(table, this, &FixtureTablePanel::OnMouseMove,
                       &FixtureTablePanel::OnMouseLeave);
  table->CallAfter([this]() {
    BindTableHoverEvents(table, this, &FixtureTablePanel::OnMouseMove,
                         &FixtureTablePanel::OnMouseLeave);
  });
  table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
              &FixtureTablePanel::OnSelectionChanged, this);

  table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
              &FixtureTablePanel::OnContextMenu, this);
  table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
              &FixtureTablePanel::OnItemActivated, this);
  table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED, &FixtureTablePanel::OnColumnSorted,
              this);

  InitializeTable();
  ReloadData();

  sizer->Add(table, 1, wxEXPAND | wxALL, 5);
  SetSizer(sizer);
}

// Releases singleton ownership when the fixture table panel is destroyed.
FixtureTablePanel::~FixtureTablePanel() {
  if (wxAuiManager *manager = wxAuiManager::GetManager(this))
    manager->DetachPane(this);
  if (HasCapture())
    ReleaseMouse();
  SetInstance(nullptr);
  store = nullptr;
}

// Configures fixture table columns and unit-aware header labels.
void FixtureTablePanel::InitializeTable() {
  columnLabels = FixtureTableColumns::DefaultLabels();
  auto &cfg = guiConfigServices->LegacyConfigManager();
  const auto distanceUnit =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  const auto weightUnit =
      Units::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
  const wxString distanceSuffix =
      wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnit));
  const wxString weightSuffix =
      wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
  columnLabels[FixtureTableColumns::ToIndex(
      FixtureTableColumns::Column::PositionX)] =
      wxString::FromUTF8(
          Units::LabelWithUnit("Pos X", std::string(distanceSuffix.ToUTF8())));
  columnLabels[FixtureTableColumns::ToIndex(
      FixtureTableColumns::Column::PositionY)] =
      wxString::FromUTF8(
          Units::LabelWithUnit("Pos Y", std::string(distanceSuffix.ToUTF8())));
  columnLabels[FixtureTableColumns::ToIndex(
      FixtureTableColumns::Column::PositionZ)] =
      wxString::FromUTF8(
          Units::LabelWithUnit("Pos Z", std::string(distanceSuffix.ToUTF8())));
  columnLabels[FixtureTableColumns::ToIndex(
      FixtureTableColumns::Column::Weight)] =
      wxString::FromUTF8(
          Units::LabelWithUnit("Weight", std::string(weightSuffix.ToUTF8())));
  FixtureTableColumns::ConfigureColumns(table, columnLabels);
}

// Rebuilds table rows from scene fixtures and reapplies validation highlights.
void FixtureTablePanel::ReloadData() {
  if (!table || !store)
    return;
  table->Freeze();
  wxWindowUpdateLocker locker(table);

  auto &cfg = guiConfigServices->LegacyConfigManager();
  const auto distanceUnit =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  const auto weightUnit =
      Units::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
  const wxString distanceSuffix =
      wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnit));
  const wxString weightSuffix =
      wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
  columnLabels[FixtureTableColumns::ToIndex(
      FixtureTableColumns::Column::PositionX)] =
      wxString::FromUTF8(
          Units::LabelWithUnit("Pos X", std::string(distanceSuffix.ToUTF8())));
  columnLabels[FixtureTableColumns::ToIndex(
      FixtureTableColumns::Column::PositionY)] =
      wxString::FromUTF8(
          Units::LabelWithUnit("Pos Y", std::string(distanceSuffix.ToUTF8())));
  columnLabels[FixtureTableColumns::ToIndex(
      FixtureTableColumns::Column::PositionZ)] =
      wxString::FromUTF8(
          Units::LabelWithUnit("Pos Z", std::string(distanceSuffix.ToUTF8())));
  columnLabels[FixtureTableColumns::ToIndex(
      FixtureTableColumns::Column::Weight)] =
      wxString::FromUTF8(
          Units::LabelWithUnit("Weight", std::string(weightSuffix.ToUTF8())));
  for (size_t i = 0; i < columnLabels.size(); ++i) {
    if (auto *column = table->GetColumn(static_cast<unsigned int>(i)))
      column->SetTitle(columnLabels[i]);
  }

  store->DeleteAllItems();
  gdtfPaths.clear();
  rowUuids.clear();
  rowUuidByKey.clear();
  gdtfPathByKey.clear();
  nextRowKey = 1;

  const auto &fixtures =
      guiConfigServices->LegacyConfigManager().GetScene().fixtures;

  std::vector<std::pair<std::string, const Fixture *>> sorted;
  sorted.reserve(fixtures.size());
  for (const auto &[uuid, fixture] : fixtures)
    sorted.emplace_back(uuid, &fixture);

  std::sort(sorted.begin(), sorted.end(), [&](const auto &A, const auto &B) {
    const Fixture *a = A.second;
    const Fixture *b = B.second;
    if (a->fixtureId != b->fixtureId)
      return a->fixtureId < b->fixtureId;
    if (a->gdtfSpec != b->gdtfSpec)
      return StringUtils::NaturalLess(a->gdtfSpec, b->gdtfSpec);
    auto addrA = FixtureTableParser::ParseAddress(a->address);
    auto addrB = FixtureTableParser::ParseAddress(b->address);
    if (addrA.universe != addrB.universe)
      return addrA.universe < addrB.universe;
    return addrA.channel < addrB.channel;
  });

  for (const auto &pair : sorted) {
    const std::string &uuid = pair.first;
    const Fixture *fixture = pair.second;
    wxVector<wxVariant> row;

    wxString name = wxString::FromUTF8(fixture->instanceName);
    long fixtureID = static_cast<long>(fixture->fixtureId);
    wxString layer = fixture->layer == DEFAULT_LAYER_NAME
                         ? wxString()
                         : wxString::FromUTF8(fixture->layer);
    long universe = 0;
    long channel = 0;
    if (!fixture->address.empty()) {
      wxStringTokenizer tk(wxString::FromUTF8(fixture->address), ".");
      if (tk.HasMoreTokens())
        tk.GetNextToken().ToLong(&universe);
      if (tk.HasMoreTokens())
        tk.GetNextToken().ToLong(&channel);
    }
    std::string fullPath;
    if (!fixture->gdtfSpec.empty()) {
      const std::string &base =
          guiConfigServices->LegacyConfigManager().GetScene().basePath;
      fs::path p = base.empty() ? fs::path(fixture->gdtfSpec)
                                : fs::path(base) / fixture->gdtfSpec;
      fullPath = p.string();
    }
    wxString gdtfFull = wxString::FromUTF8(fullPath);
    gdtfPaths.push_back(gdtfFull);
    wxString gdtf = wxFileName(gdtfFull).GetFullName();
    wxString type = wxString::FromUTF8(fixture->typeName);
    if (type.empty())
      type = wxFileName(gdtfFull).GetName();
    wxString mode = wxString::FromUTF8(fixture->gdtfMode);

    int chCount = GetGdtfModeChannelCount(std::string(gdtfFull.ToUTF8()),
                                          fixture->gdtfMode);
    wxString chCountStr =
        chCount >= 0 ? wxString::Format("%d", chCount) : wxString();

    auto posArr = fixture->GetPosition();
    wxString posX = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        posArr[0], distanceUnit, Units::ValueFormatContext::Table));
    wxString posY = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        posArr[1], distanceUnit, Units::ValueFormatContext::Table));
    wxString posZ = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        posArr[2], distanceUnit, Units::ValueFormatContext::Table));
    wxString posName = wxString::FromUTF8(fixture->positionName);

    auto euler = MatrixUtils::MatrixToEuler(fixture->transform);
    wxString roll = wxString::Format("%.1f", euler[2]) + DegreeSymbol();
    wxString pitch = wxString::Format("%.1f", euler[1]) + DegreeSymbol();
    wxString yaw = wxString::Format("%.1f", euler[0]) + DegreeSymbol();

    row.push_back(fixtureID);
    row.push_back(name);
    row.push_back(type);
    row.push_back(layer);
    row.push_back(posName);
    row.push_back(universe);
    row.push_back(channel);
    row.push_back(mode);
    row.push_back(chCountStr);
    row.push_back(gdtf);
    row.push_back(posX);
    row.push_back(posY);
    row.push_back(posZ);
    row.push_back(roll);
    row.push_back(pitch);
    row.push_back(yaw);
    wxString power = wxString::Format("%.1f", fixture->powerConsumptionW);
    wxString weight = wxString::FromUTF8(Units::FormatWeightFromKilograms(
        fixture->weightKg, weightUnit, Units::ValueFormatContext::Table));
    wxString category = wxString::FromUTF8(fixture->category);
    row.push_back(power);
    row.push_back(weight);
    row.push_back(category);
    wxString color = wxString::FromUTF8(fixture->visualColorHex);
    if (!color.IsEmpty()) {
      wxColour col(color);
      wxBitmap bmp(16, 16);
      {
        wxMemoryDC dc(bmp);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(col));
        dc.DrawRectangle(0, 0, 16, 16);
        dc.SelectObject(wxNullBitmap);
      }
      wxDataViewIconText iconText(color, bmp);
      wxVariant var;
      var << iconText;
      row.push_back(var);
    } else {
      wxVariant var;
      var << wxDataViewIconText();
      row.push_back(var);
    }
    wxString mvrColor = wxString::FromUTF8(fixture->mvrFixtureColorHex);
    if (!mvrColor.IsEmpty()) {
      wxColour col(mvrColor);
      wxBitmap bmp(16, 16);
      {
        wxMemoryDC dc(bmp);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(col));
        dc.DrawRectangle(0, 0, 16, 16);
        dc.SelectObject(wxNullBitmap);
      }
      wxVariant var;
      var << wxDataViewIconText(mvrColor, bmp);
      row.push_back(var);
    } else {
      wxVariant var;
      var << wxDataViewIconText();
      row.push_back(var);
    }
    const wxUIntPtr rowKey = nextRowKey++;
    store->AppendItem(row, rowKey);
    rowUuids.push_back(uuid);
    rowUuidByKey[rowKey] = uuid;
    gdtfPathByKey[rowKey] = gdtfFull;
  }

  if (Viewer3DPanel::Instance())
    Viewer3DPanel::Instance()->SetSelectedFixtures({});

  RunValidationHighlights(SceneDataUpdateType::kGeneral);

  // Let wxDataViewListCtrl manage column headers and sorting
  if (LayerPanel::Instance())
    LayerPanel::Instance()->ReloadLayers();
  if (SummaryPanel::Instance() && IsActivePage())
    SummaryPanel::Instance()->ShowFixtureSummary();
  table->Thaw();
}

// Handles context-menu editing actions and only applies scene updates when data
// actually changes.
void FixtureTablePanel::OnContextMenu(wxDataViewEvent &event) {
  wxDataViewItem item = event.GetItem();
  int col = event.GetColumn();
  const auto namedColumn = FixtureTableColumns::FromIndex(col);
  if (!item.IsOk() || !namedColumn ||
      static_cast<size_t>(col) >= columnLabels.size())
    return;

  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (selections.empty())
    selections.push_back(item);

  std::vector<std::string> selectedUuids;
  for (const auto &itSel : selections) {
    const std::string uuid = UuidForItem(itSel);
    if (!uuid.empty())
      selectedUuids.push_back(uuid);
  }
  std::vector<std::string> oldOrder = rowUuids;

  // Model file column opens file dialog
  if (col ==
      FixtureTableColumns::ToIndex(FixtureTableColumns::Column::ModelFile)) {
    bool changed = false;
    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() == wxID_OK) {
      wxString path = fdlg.GetPath();
      float w = 0.0f, p = 0.0f;
      std::string pathUtf8(path.ToUTF8());
      GetGdtfProperties(pathUtf8, w, p);
      wxString typeName = wxString::FromUTF8(GetGdtfFixtureName(pathUtf8));
      if (typeName.empty())
        typeName = wxFileName(path).GetName();
      wxString fileName = fdlg.GetFilename();
      const std::string typeNameUtf8 = std::string(typeName.ToUTF8());
      const auto dictColor = GdtfDictionary::Get(typeNameUtf8);

      std::vector<std::string> prevTypes;
      std::unordered_set<std::string> prevTypeSet;

      for (const auto &itSel : selections) {
        int r = table->ItemToRow(itSel);
        if (r == wxNOT_FOUND)
          continue;

        wxVariant prevType;
        table->GetValue(
            prevType, r,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));
        const std::string prevTypeUtf8 =
            std::string(prevType.GetString().ToUTF8());
        prevTypes.push_back(prevTypeUtf8);
        if (!prevTypeUtf8.empty())
          prevTypeSet.insert(prevTypeUtf8);

        if ((size_t)r >= gdtfPaths.size())
          gdtfPaths.resize(table->GetItemCount());

        wxVariant existingModelFile;
        table->GetValue(existingModelFile, r,
                        FixtureTableColumns::ToIndex(
                            FixtureTableColumns::Column::ModelFile));
        wxVariant existingTypeName;
        table->GetValue(
            existingTypeName, r,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));

        const bool rowChanged = gdtfPaths[static_cast<size_t>(r)] != path ||
                                existingModelFile.GetString() != fileName ||
                                existingTypeName.GetString() != typeName;
        if (!rowChanged)
          continue;

        SetGdtfPathForRow(static_cast<unsigned int>(r), path);
        table->SetValue(wxVariant(fileName), r,
                        FixtureTableColumns::ToIndex(
                            FixtureTableColumns::Column::ModelFile));
        table->SetValue(
            wxVariant(typeName), r,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));

        wxString pstr = wxString::Format("%.1f", p);
        wxString wstr = wxString::Format("%.2f", w);
        table->SetValue(
            wxVariant(pstr), r,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Power));
        table->SetValue(
            wxVariant(wstr), r,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Weight));
        if (dictColor && !dictColor->visualColorHex.empty())
          SetFixtureColorCell(table, r, dictColor->visualColorHex);
        changed = true;
      }

      for (unsigned int i = 0; i < table->GetItemCount(); ++i) {
        wxVariant typeVar;
        table->GetValue(
            typeVar, i,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));
        const std::string currentType =
            std::string(typeVar.GetString().ToUTF8());
        if (prevTypeSet.find(currentType) == prevTypeSet.end())
          continue;

        if ((size_t)i >= gdtfPaths.size())
          gdtfPaths.resize(table->GetItemCount());

        wxVariant existingModelFile;
        table->GetValue(existingModelFile, i,
                        FixtureTableColumns::ToIndex(
                            FixtureTableColumns::Column::ModelFile));
        wxVariant existingTypeName;
        table->GetValue(
            existingTypeName, i,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));

        const bool rowChanged = gdtfPaths[static_cast<size_t>(i)] != path ||
                                existingModelFile.GetString() != fileName ||
                                existingTypeName.GetString() != typeName;
        if (!rowChanged)
          continue;

        SetGdtfPathForRow(i, path);
        table->SetValue(wxVariant(fileName), i,
                        FixtureTableColumns::ToIndex(
                            FixtureTableColumns::Column::ModelFile));
        table->SetValue(
            wxVariant(typeName), i,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));

        wxString pstr = wxString::Format("%.1f", p);
        wxString wstr = wxString::Format("%.2f", w);
        table->SetValue(
            wxVariant(pstr), i,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Power));
        table->SetValue(
            wxVariant(wstr), i,
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Weight));
        if (dictColor && !dictColor->visualColorHex.empty())
          SetFixtureColorCell(table, i, dictColor->visualColorHex);
        changed = true;
      }

      PropagateTypeValues(selections, FixtureTableColumns::ToIndex(
                                          FixtureTableColumns::Column::Power));
      PropagateTypeValues(selections, FixtureTableColumns::ToIndex(
                                          FixtureTableColumns::Column::Weight));

      wxString dictMode;
      if (!prevTypes.empty()) {
        if (auto entry = GdtfDictionary::Get(prevTypes[0]))
          dictMode = wxString::FromUTF8(entry->mode);
      }
      if (changed)
        ApplyModeForGdtf(path, dictMode);

      // Project table GDTF changes stay project-scoped and do not promote files
      // into the user library.
    }
    if (!changed)
      return;
    ResyncRows(oldOrder, selectedUuids);
    const auto updateType =
        CombineUpdateTypes(UpdateTypeForColumn(FixtureTableColumns::ToIndex(
                               FixtureTableColumns::Column::ModelFile)),
                           UpdateTypeForColumn(FixtureTableColumns::ToIndex(
                               FixtureTableColumns::Column::Power)));
    UpdateSceneData(true, updateType);
    RefreshViewersForFixtureUpdate(updateType);
    if (MainWindow::Instance()) {
      MainWindow::Instance()->RequestFixtureSymbolAutoUpdate();
    }
    return;
  }

  // Mode column shows available modes of the selected GDTF
  if (col == FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Mode)) {
    int r = table->ItemToRow(item);
    if (r == wxNOT_FOUND)
      return;

    wxString gdtfPath;
    if ((size_t)r < gdtfPaths.size())
      gdtfPath = gdtfPaths[r];

    std::vector<std::string> modes =
        GetGdtfModes(std::string(gdtfPath.ToUTF8()));
    if (modes.size() <= 1)
      return;

    wxArrayString choices;
    for (const auto &m : modes)
      choices.push_back(wxString::FromUTF8(m));

    wxSingleChoiceDialog dlg(this, "Select DMX mode", "DMX Mode", choices);
    if (dlg.ShowModal() != wxID_OK)
      return;

    wxString sel = dlg.GetStringSelection();

    wxDataViewItemArray modeSelections;
    table->GetSelections(modeSelections);
    if (modeSelections.empty())
      modeSelections.push_back(item);

    for (const auto &itSel : modeSelections) {
      int sr = table->ItemToRow(itSel);
      if (sr == wxNOT_FOUND)
        continue;
      if ((size_t)sr >= gdtfPaths.size())
        continue;
      if (gdtfPaths[sr] != gdtfPath)
        continue;

      table->SetValue(wxVariant(sel), sr, col);

      int chCount = GetGdtfModeChannelCount(std::string(gdtfPath.ToUTF8()),
                                            std::string(sel.ToUTF8()));
      wxString chStr =
          chCount >= 0 ? wxString::Format("%d", chCount) : wxString();
      table->SetValue(wxVariant(chStr), sr,
                      FixtureTableColumns::ToIndex(
                          FixtureTableColumns::Column::ChannelCount));
    }
    ApplyModeForGdtf(gdtfPath, sel);

    ResyncRows(oldOrder, selectedUuids);
    const auto updateType =
        CombineUpdateTypes(UpdateTypeForColumn(FixtureTableColumns::ToIndex(
                               FixtureTableColumns::Column::Mode)),
                           UpdateTypeForColumn(FixtureTableColumns::ToIndex(
                               FixtureTableColumns::Column::ChannelCount)));
    UpdateSceneData(true, updateType);
    RefreshViewersForFixtureUpdate(updateType);
    return;
  }

  // Layer column uses existing layer list
  if (col == FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Layer)) {
    auto layers = guiConfigServices->LegacyConfigManager().GetLayerNames();
    wxArrayString choices;
    for (const auto &n : layers)
      choices.push_back(wxString::FromUTF8(n));
    wxSingleChoiceDialog dlg(this, "Select layer", "Layer", choices);
    if (dlg.ShowModal() != wxID_OK)
      return;
    wxString sel = dlg.GetStringSelection();
    wxString val =
        sel == wxString::FromUTF8(DEFAULT_LAYER_NAME) ? wxString() : sel;
    for (const auto &itSel : selections) {
      int r = table->ItemToRow(itSel);
      if (r != wxNOT_FOUND)
        table->SetValue(wxVariant(val), r, col);
    }
    PropagateTypeValues(selections, col);
    const auto updateType = UpdateTypeForColumn(col);
    UpdateSceneData(true, updateType);
    return;
  }

  // Channel column edits both universe and channel
  if (col ==
      FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Channel)) {
    int r = table->ItemToRow(item);
    if (r == wxNOT_FOUND)
      return;

    wxVariant vUni, vCh;
    table->GetValue(
        vUni, r,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Universe));
    table->GetValue(
        vCh, r,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Channel));
    AddressDialog dlg(this, vUni.GetLong(), vCh.GetLong());
    if (dlg.ShowModal() != wxID_OK)
      return;

    int newUni = dlg.GetUniverse();
    int newCh = dlg.GetChannel();
    if (newCh < 1)
      newCh = 1;
    if (newCh > 512) {
      newUni += (newCh - 1) / 512;
      newCh = 1;
    }

    wxDataViewItemArray adrSelections;
    table->GetSelections(adrSelections);
    if (adrSelections.empty())
      adrSelections.push_back(item);

    std::vector<int> selectedRows;
    for (const auto &itSel : adrSelections) {
      int row = table->ItemToRow(itSel);
      if (row != wxNOT_FOUND)
        selectedRows.push_back(row);
    }

    auto orderedRows =
        FixtureTableEditService::BuildOrderedRows(selectedRows, selectionOrder);

    std::vector<int> counts;
    counts.reserve(orderedRows.size());
    for (int row : orderedRows) {
      wxVariant vCount;
      table->GetValue(vCount, row,
                      FixtureTableColumns::ToIndex(
                          FixtureTableColumns::Column::ChannelCount));
      long c = 1;
      if (!vCount.GetString().ToLong(&c))
        c = 1;
      if (c < 1)
        c = 1;
      counts.push_back(static_cast<int>(c));
    }

    auto addrs = PatchManager::SequentialPatch(counts, newUni, newCh);
    for (size_t i = 0; i < orderedRows.size() && i < addrs.size(); ++i) {
      table->SetValue(
          wxVariant(addrs[i].universe), orderedRows[i],
          FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Universe));
      table->SetValue(
          wxVariant(addrs[i].channel), orderedRows[i],
          FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Channel));
    }

    ResyncRows(oldOrder, selectedUuids);
    const auto updateType =
        CombineUpdateTypes(UpdateTypeForColumn(FixtureTableColumns::ToIndex(
                               FixtureTableColumns::Column::Universe)),
                           UpdateTypeForColumn(FixtureTableColumns::ToIndex(
                               FixtureTableColumns::Column::Channel)));
    UpdateSceneData(true, updateType);
    RefreshViewersForFixtureUpdate(updateType);
    return;
  }

  int baseRow = table->ItemToRow(item);
  if (baseRow == wxNOT_FOUND)
    return;

  wxVariant current;
  table->GetValue(current, baseRow, col);

  if (col ==
      FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Category)) {
    const wxArrayString choices = {
        "Beam",   "Blinder", "Conventional", "FX",    "Hoist",
        "Hybrid", "Laser",   "LED",          "Smoke", "Spot",
        "Strobe", "Unknown", "Video",        "Wash"};
    wxSingleChoiceDialog dlg(this, "Select category", "Category", choices);
    if (!current.GetString().empty()) {
      int sel = choices.Index(current.GetString());
      if (sel != wxNOT_FOUND)
        dlg.SetSelection(sel);
    }
    if (dlg.ShowModal() != wxID_OK)
      return;
    const wxString value = dlg.GetStringSelection();
    std::unordered_set<std::string> selectedTypes;
    for (const auto &it : selections) {
      int r = table->ItemToRow(it);
      if (r == wxNOT_FOUND)
        continue;
      wxVariant typeValue;
      table->GetValue(
          typeValue, r,
          FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));
      selectedTypes.insert(std::string(typeValue.GetString().ToUTF8()));
    }

    std::vector<unsigned int> affectedRows;
    affectedRows.reserve(table->GetItemCount());
    for (unsigned int row = 0; row < table->GetItemCount(); ++row) {
      wxVariant typeValue;
      table->GetValue(
          typeValue, row,
          FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));
      const std::string typeName = std::string(typeValue.GetString().ToUTF8());
      if (selectedTypes.find(typeName) != selectedTypes.end())
        affectedRows.push_back(row);
    }

    {
      wxWindowUpdateLocker locker(table);
      for (const auto row : affectedRows) {
        wxVariant currentValue;
        table->GetValue(currentValue, row, col);
        if (currentValue.GetString() == value)
          continue;

        table->SetValue(wxVariant(value), row, col);
      }
    }

    for (const auto row : affectedRows) {
      if (row < rowUuids.size())
        manualCategoryUuidsPending.insert(rowUuids[row]);
    }

    const auto updateType = UpdateTypeForColumn(col);
    UpdateSceneData(true, updateType, &affectedRows);

    auto &fixtures =
        guiConfigServices->LegacyConfigManager().GetScene().fixtures;
    for (const auto row : affectedRows) {
      if (row >= table->GetItemCount())
        continue;
      store->ClearCellTextColour(
          row,
          FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Category));
      if (row >= rowUuids.size())
        continue;
      const auto itFixture = fixtures.find(rowUuids[row]);
      if (itFixture == fixtures.end())
        continue;
      if (itFixture->second.categorySource ==
          GdtfFixtureCategory::kAutoFallbackSource) {
        store->SetCellTextColour(row, 18, *wxRED);
      }
    }
    return;
  }

  const bool isVisualColor =
      col ==
      FixtureTableColumns::ToIndex(FixtureTableColumns::Column::VisualColor);
  const bool isColorFilter =
      col == FixtureTableColumns::ToIndex(
                 FixtureTableColumns::Column::MvrColor);
  if (isVisualColor || isColorFilter) {
    wxColourData data;
    data.SetChooseFull(true);
    wxColour initial(current.GetString());
    data.SetColour(initial);
    wxColourDialog cdlg(this, &data);
    if (cdlg.ShowModal() != wxID_OK)
      return;
    wxColour selCol = cdlg.GetColourData().GetColour();
    wxString value = selCol.GetAsString(wxC2S_HTML_SYNTAX);
    wxBitmap bmp(16, 16);
    {
      wxMemoryDC dc(bmp);
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(selCol));
      dc.DrawRectangle(0, 0, 16, 16);
      dc.SelectObject(wxNullBitmap);
    }
    wxDataViewIconText iconText(value, bmp);
    wxVariant var;
    var << iconText;
    for (const auto &it : selections) {
      int r = table->ItemToRow(it);
      if (r != wxNOT_FOUND)
        table->SetValue(var, r, col);
    }
    if (isVisualColor)
      PropagateTypeValues(selections, col);
    ResyncRows(oldOrder, selectedUuids);
    const auto updateType = UpdateTypeForColumn(col);
    UpdateSceneData(true, updateType);
    RefreshViewersForFixtureUpdate(updateType);
    return;
  }

  wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col],
                        current.GetString());
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString raw = dlg.GetValue();
  bool trailingSpace = raw.EndsWith(" ");
  wxString value = raw.Trim(true).Trim(false);

  const bool intCol = FixtureTableColumns::IsInteger(*namedColumn);
  const bool numericCol =
      intCol || FixtureTableColumns::IsNumeric(*namedColumn);
  bool relative = false;
  double delta = 0.0;
  if (!intCol && FixtureTableColumns::IsTransform(*namedColumn) &&
      (value.StartsWith("++") || value.StartsWith("--"))) {
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
        if (FixtureTableColumns::IsRotation(*namedColumn)) {
          if (!DegreeSymbol().empty())
            cur.Replace(DegreeSymbol(), "");
        }
        double curVal = 0.0;
        cur.ToDouble(&curVal);
        double newVal = curVal + delta;
        wxString out;
        if (FixtureTableColumns::IsRotation(*namedColumn))
          out = wxString::Format("%.1f", newVal) + DegreeSymbol();
        else
          out = wxString::Format("%.3f", newVal);
        table->SetValue(wxVariant(out), r, col);
      }
    } else {
      auto range = FixtureTableParser::SplitRangeParts(value);
      wxArrayString parts = range.parts;
      if (parts.size() == 0 || parts.size() > 2) {
        wxMessageBox("Invalid numeric value", "Error", wxOK | wxICON_ERROR);
        return;
      }
      if (range.usedSeparator && parts.size() != 2 &&
          !(parts.size() == 1 && range.trailingSeparator)) {
        wxMessageBox("Invalid numeric value", "Error", wxOK | wxICON_ERROR);
        return;
      }

      if (intCol) {
        long v1, v2 = 0;
        if (!parts[0].ToLong(&v1)) {
          wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
          return;
        }
        if (col == FixtureTableColumns::ToIndex(
                       FixtureTableColumns::Column::Channel) &&
            (v1 < 1 || v1 > 512)) {
          wxMessageBox("Channel out of range (1-512)", "Error",
                       wxOK | wxICON_ERROR);
          return;
        }
        bool interp = false;
        bool sequential = false;
        if (parts.size() == 2) {
          if (!parts[1].ToLong(&v2)) {
            wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
            return;
          }
          if (col == FixtureTableColumns::ToIndex(
                         FixtureTableColumns::Column::Channel) &&
              (v2 < 1 || v2 > 512)) {
            wxMessageBox("Channel out of range (1-512)", "Error",
                         wxOK | wxICON_ERROR);
            return;
          }
          interp = selections.size() > 1;
        } else if ((trailingSpace && !range.usedSeparator) ||
                   (range.usedSeparator && range.trailingSeparator)) {
          sequential = selections.size() > 1;
        }

        std::vector<int> selectedRows;
        selectedRows.reserve(selections.size());
        for (const auto &it : selections) {
          int r = table->ItemToRow(it);
          if (r != wxNOT_FOUND)
            selectedRows.push_back(r);
        }

        auto orderedRows = FixtureTableEditService::BuildOrderedRows(
            selectedRows, selectionOrder);

        for (size_t i = 0; i < orderedRows.size(); ++i) {
          long val = v1;
          if (interp)
            val = static_cast<long>(v1 + (double)(v2 - v1) * i /
                                             (orderedRows.size() - 1));
          else if (sequential)
            val = v1 + static_cast<long>(i);

          table->SetValue(wxVariant(val), orderedRows[i], col);
        }
      } else // floating point stored as string
      {
        double v1, v2 = 0.0;
        if (!parts[0].ToDouble(&v1)) {
          wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
          return;
        }
        bool interp = false;
        bool sequential = false;
        if (parts.size() == 2) {
          if (!parts[1].ToDouble(&v2)) {
            wxMessageBox("Invalid value", "Error", wxOK | wxICON_ERROR);
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
          if (FixtureTableColumns::IsRotation(*namedColumn))
            out = wxString::Format("%.1f", val) + DegreeSymbol();
          else
            out = wxString::Format("%.3f", val);

          int r = table->ItemToRow(selections[i]);
          if (r != wxNOT_FOUND)
            table->SetValue(wxVariant(out), r, col);
        }
      }
    }
  } else {
    if (col ==
            FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Name) &&
        selections.size() > 1) {
      int spacePos = value.find_last_of(' ');
      long baseNum = 0;
      if (spacePos != wxNOT_FOUND && value.Mid(spacePos + 1).ToLong(&baseNum)) {
        wxString prefix = value.Left(spacePos);

        std::vector<int> selectedRows;
        selectedRows.reserve(selections.size());
        for (const auto &it : selections) {
          int r = table->ItemToRow(it);
          if (r != wxNOT_FOUND)
            selectedRows.push_back(r);
        }

        auto orderedRows = FixtureTableEditService::BuildOrderedRows(
            selectedRows, selectionOrder);

        for (size_t i = 0; i < orderedRows.size(); ++i) {
          wxString newName =
              prefix + " " + wxString::Format("%ld", baseNum + (long)i);
          table->SetValue(wxVariant(newName), orderedRows[i], col);
        }
      } else {
        for (const auto &it : selections) {
          int r = table->ItemToRow(it);
          if (r != wxNOT_FOUND)
            table->SetValue(wxVariant(value), r, col);
        }
      }
    } else {
      for (const auto &it : selections) {
        int r = table->ItemToRow(it);
        if (r != wxNOT_FOUND)
          table->SetValue(wxVariant(value), r, col);
      }
    }
  }
  PropagateTypeValues(selections, col);
  const SceneDataUpdateType updateType = UpdateTypeForColumn(col);
  ResyncRows(oldOrder, selectedUuids);
  if (col == FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Name)) {
    std::vector<int> selectedRows;
    selectedRows.reserve(selections.size());
    for (const auto &it : selections) {
      int r = table->ItemToRow(it);
      if (r != wxNOT_FOUND)
        selectedRows.push_back(r);
    }
    ConfigManagerSceneAdapter adapter;
    FixtureTableEditService::ApplyNameChanges(adapter, table, rowUuids,
                                              selectedRows, true);
  } else {
    UpdateSceneData(true, updateType);
  }
  RefreshViewersForFixtureUpdate(updateType);
}

static FixtureTablePanel *s_instance = nullptr;

// Returns the current fixture table panel singleton instance.
FixtureTablePanel *FixtureTablePanel::Instance() { return s_instance; }

// Sets the fixture table panel singleton instance pointer.
void FixtureTablePanel::SetInstance(FixtureTablePanel *panel) {
  s_instance = panel;
}

// Maps a table column index to its corresponding scene update category.
FixtureTablePanel::SceneDataUpdateType
FixtureTablePanel::UpdateTypeForColumn(int column) {
  return UpdateTypeForColumnImpl(column);
}

// Combines two scene update categories into a safe aggregate update.
FixtureTablePanel::SceneDataUpdateType
FixtureTablePanel::CombineUpdateTypes(SceneDataUpdateType lhs,
                                      SceneDataUpdateType rhs) {
  return CombineUpdateTypesImpl(lhs, rhs);
}

// Reports whether an update scope requires full viewer scene rebuild.
bool FixtureTablePanel::RequiresFullViewerSceneUpdate(
    SceneDataUpdateType updateType) {
  switch (updateType) {
  case SceneDataUpdateType::kPatchOnly:
  case SceneDataUpdateType::kTransformOnly:
  case SceneDataUpdateType::kWeightOrPosition:
  case SceneDataUpdateType::kGeneral:
    return true;
  case SceneDataUpdateType::kVisualLabelOnly:
  case SceneDataUpdateType::kAppearanceOnly:
  case SceneDataUpdateType::kCategoryOnly:
  case SceneDataUpdateType::kMetadataOnly:
  case SceneDataUpdateType::kFixtureIdOnly:
    return false;
  }
  return true;
}

// Checks whether the fixture table panel is the active notebook page.
bool FixtureTablePanel::IsActivePage() const {
  auto *nb = dynamic_cast<wxNotebook *>(GetParent());
  return nb && nb->GetPage(nb->GetSelection()) == this;
}

// Applies a primary hover highlight to one fixture row.
void FixtureTablePanel::HighlightFixture(const std::string &uuid) {
  HighlightFixture(uuid, {});
}

// Applies primary and related group-hover highlights to fixture rows.
void FixtureTablePanel::HighlightFixture(
    const std::string &uuid, const std::vector<std::string> &relatedUuids) {
  if (uuid == highlightedUuid && relatedUuids == highlightedRelatedUuids)
    return;

  auto findRow = [&](const std::string &candidate) -> int {
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
  for (const auto &relatedUuid : relatedUuids) {
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

// Highlights fixtures with duplicate DMX universe/channel patch addresses.
void FixtureTablePanel::HighlightPatchConflicts() {
  // Clear previous highlighting on Universe and Channel columns
  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    store->ClearCellTextColour(
        i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Universe));
    store->ClearCellTextColour(
        i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Channel));
  }

  struct PatchInfo {
    int start;
    int end;
    unsigned row;
  };
  std::unordered_map<int, std::vector<PatchInfo>> uniMap;

  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    wxVariant v;
    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Universe));
    long uni = v.GetLong();
    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Channel));
    long ch = v.GetLong();
    table->GetValue(v, i,
                    FixtureTableColumns::ToIndex(
                        FixtureTableColumns::Column::ChannelCount));
    long count = 1;
    if (!v.GetString().ToLong(&count))
      count = 1;

    if (uni <= 0 || ch <= 0 || count <= 0)
      continue;

    PatchInfo info{static_cast<int>(ch), static_cast<int>(ch + count - 1), i};
    uniMap[static_cast<int>(uni)].push_back(info);
  }

  for (auto &[uni, vec] : uniMap) {
    std::sort(vec.begin(), vec.end(),
              [](const PatchInfo &a, const PatchInfo &b) {
                return a.start < b.start;
              });

    for (size_t i = 0; i < vec.size(); ++i) {
      for (size_t j = i + 1; j < vec.size(); ++j) {
        if (vec[j].start <= vec[i].end) {
          store->SetCellTextColour(vec[i].row, 5, *wxRED);
          store->SetCellTextColour(vec[i].row, 6, *wxRED);
          store->SetCellTextColour(vec[j].row, 5, *wxRED);
          store->SetCellTextColour(vec[j].row, 6, *wxRED);
        } else {
          break;
        }
      }
    }
  }
}

// Clears all current table row selections.
void FixtureTablePanel::ClearSelection() {
  table->UnselectAll();
  selectionOrder.clear();
  UpdateSelectionHighlight();
}

// Returns UUIDs for all currently selected fixture rows.
std::vector<std::string> FixtureTablePanel::GetSelectedUuids() const {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<std::string> uuids;
  uuids.reserve(selections.size());
  for (const auto &it : selections) {
    const std::string uuid = UuidForItem(it);
    if (!uuid.empty())
      uuids.push_back(uuid);
  }
  return uuids;
}

// Selects rows matching the provided fixture UUID list.
void FixtureTablePanel::SelectByUuid(const std::vector<std::string> &uuids,
                                     bool notifySelectionChanged) {
  RebuildRowCachesFromRowKeys();
  std::unique_ptr<wxEventBlocker> selectionBlocker;
  if (!notifySelectionChanged)
    selectionBlocker = std::make_unique<wxEventBlocker>(
        table, wxEVT_DATAVIEW_SELECTION_CHANGED);
  table->UnselectAll();
  selectionOrder.clear();
  std::vector<bool> selectedRows(table->GetItemCount(), false);
  for (const auto &u : uuids) {
    auto pos = std::find(rowUuids.begin(), rowUuids.end(), u);
    if (pos != rowUuids.end()) {
      int row = static_cast<int>(pos - rowUuids.begin());
      table->SelectRow(row);
      selectionOrder.push_back(row);
      if (row >= 0 && static_cast<size_t>(row) < selectedRows.size())
        selectedRows[row] = true;
    }
  }
  store->SetSelectedRows(selectedRows);
}

// Deletes selected fixtures from the scene and refreshes related panels.
void FixtureTablePanel::DeleteSelected(bool pushUndoState) {
  RebuildRowCachesFromRowKeys();
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (selections.empty())
    return;

  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  if (pushUndoState)
    cfg.PushUndoState("delete fixture");
  cfg.SetSelectedFixtures({});

  std::vector<std::string> oldOrder = rowUuids;
  std::vector<wxString> oldPaths = gdtfPaths;

  std::vector<int> rows;
  rows.reserve(selections.size());
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND)
      rows.push_back(r);
  }
  std::sort(rows.begin(), rows.end(), std::greater<int>());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

  auto &scene = guiConfigServices->LegacyConfigManager().GetScene();
  for (int r : rows) {
    if ((size_t)r < rowUuids.size()) {
      wxDataViewItem rowItem = table->RowToItem(static_cast<unsigned int>(r));
      const wxUIntPtr rowKey = store->GetItemData(rowItem);
      scene.fixtures.erase(rowUuids[r]);
      rowUuids.erase(rowUuids.begin() + r);
      if ((size_t)r < gdtfPaths.size())
        gdtfPaths.erase(gdtfPaths.begin() + r);
      rowUuidByKey.erase(rowKey);
      gdtfPathByKey.erase(rowKey);
      store->DeleteItem(r);
      for (auto itSel = selectionOrder.begin();
           itSel != selectionOrder.end();) {
        if (*itSel == r)
          itSel = selectionOrder.erase(itSel);
        else {
          if (*itSel > r)
            --(*itSel);
          ++itSel;
        }
      }
    }
  }

  RunValidationHighlights(SceneDataUpdateType::kGeneral);

  std::vector<std::string> mergedSelection;
  const auto appendSelection = [&](const std::vector<std::string> &source) {
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
    SummaryPanel::Instance()->ShowFixtureSummary();

  if (RiggingPanel::Instance())
    RiggingPanel::Instance()->RefreshData();

  selectionOrder.clear();
  ResyncRows(oldOrder, {}, &oldPaths);
}

// Commits inline cell edits and propagates resulting scene updates.
void FixtureTablePanel::OnItemActivated(wxDataViewEvent &event) {
  wxDataViewItem item = event.GetItem();
  if (!item.IsOk()) {
    event.Skip();
    return;
  }

  // On macOS, opening a modal editor from a double-click can happen while a
  // drag-selection capture is still active. Releasing capture before opening
  // the dialog avoids a later destruction-time assert/crash.
  if (dragSelecting) {
    dragSelecting = false;
    if (HasCapture())
      ReleaseMouse();
  }

  int r = table->ItemToRow(item);
  if (r == wxNOT_FOUND)
    return;

  struct Viewer3DModalGuard {
    explicit Viewer3DModalGuard(Viewer3DPanel *panel) : panel(panel) {
      if (panel)
        panel->SetModalDialogActive(true);
    }
    ~Viewer3DModalGuard() {
      if (panel)
        panel->SetModalDialogActive(false);
    }
    Viewer3DPanel *panel = nullptr;
  } viewer3DModalGuard(Viewer3DPanel::Instance());

  FixtureEditDialog dlg(this, r);
  dlg.ShowModal();
}

// Captures mouse focus for drag-style interactions inside the table.
void FixtureTablePanel::OnLeftDown(wxMouseEvent &evt) { evt.Skip(); }

// Releases mouse capture after table drag-style interactions finish.
void FixtureTablePanel::OnLeftUp(wxMouseEvent &evt) { evt.Skip(); }

// Resets capture state when the system forces mouse capture loss.
void FixtureTablePanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(evt)) {
  dragSelecting = false;
  startRow = wxNOT_FOUND;
}

// Updates hover tooltip state while the pointer moves over table cells.
void FixtureTablePanel::OnMouseMove(wxMouseEvent &evt) {
  UpdateHoverTooltip(NormalizeMousePositionForTable(table, evt));
  evt.Skip();
}

// Clears hover tooltip state when the pointer leaves the table area.
void FixtureTablePanel::OnMouseLeave(wxMouseEvent &evt) {
  if (!activeHoverTooltip.IsEmpty()) {
    SetTableAndChildTooltips(table, wxString());
    activeHoverTooltip.clear();
  }
  evt.Skip();
}

// Shows contextual tooltip text for validation-marked table cells.
void FixtureTablePanel::UpdateHoverTooltip(const wxPoint &position) {
  wxDataViewItem item;
  wxDataViewColumn *column = nullptr;
  table->HitTest(position, item, column);

  wxString tooltip;
  if (item.IsOk() && column) {
    int row = table->ItemToRow(item);
    int modelColumn = column->GetModelColumn();
    if (IsRedCell(store, row, modelColumn)) {
      if (modelColumn == 18 && row >= 0 &&
          static_cast<size_t>(row) < rowUuids.size()) {
        const auto &fixtures =
            guiConfigServices->LegacyConfigManager().GetScene().fixtures;
        auto it = fixtures.find(rowUuids[static_cast<size_t>(row)]);
        if (it != fixtures.end())
          tooltip = BuildCategoryFallbackTooltip(it->second);
      } else {
        tooltip = BuildFixtureTooltipForColumn(modelColumn);
      }
    }
  }

  if (tooltip == activeHoverTooltip)
    return;

  SetTableAndChildTooltips(table, tooltip);
  activeHoverTooltip = tooltip;
}

// Syncs cross-panel selection state when table selection changes.
void FixtureTablePanel::OnSelectionChanged(wxDataViewEvent &evt) {
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
  std::vector<int> currentRows;
  currentRows.reserve(selections.size());
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND && (size_t)r < rowUuids.size()) {
      currentRows.push_back(r);
    }
  }
  // Preserve existing order but drop unselected rows
  std::vector<int> newOrder;
  for (int r : selectionOrder)
    if (std::find(currentRows.begin(), currentRows.end(), r) !=
        currentRows.end())
      newOrder.push_back(r);
  // Append newly selected rows in the order reported
  for (int r : currentRows)
    if (std::find(newOrder.begin(), newOrder.end(), r) == newOrder.end())
      newOrder.push_back(r);
  selectionOrder.swap(newOrder);

  std::vector<std::string> orderedUuids;
  orderedUuids.reserve(selectionOrder.size());
  for (int rowIndex : selectionOrder) {
    if (rowIndex >= 0 && static_cast<size_t>(rowIndex) < rowUuids.size())
      orderedUuids.push_back(rowUuids[static_cast<size_t>(rowIndex)]);
  }
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  if (orderedUuids != cfg.GetSelectedFixtures()) {
    cfg.PushUndoState("fixture selection");
    cfg.SetSelectedFixtures(orderedUuids);
  }
  std::vector<std::string> mergedSelection;
  const auto appendSelection = [&](const std::vector<std::string> &source) {
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

// Reapplies row highlight styling based on current selection state.
void FixtureTablePanel::UpdateSelectionHighlight() {
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

// Normalizes edited position values and tracks whether any row changed.
void FixtureTablePanel::UpdatePositionValues(
    const std::vector<std::string> &uuids) {
  if (!table)
    return;

  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  auto &scene = cfg.GetScene();
  wxWindowUpdateLocker locker(table);

  for (const auto &uuid : uuids) {
    auto it = scene.fixtures.find(uuid);
    if (it == scene.fixtures.end())
      continue;

    auto posArr = it->second.GetPosition();
    wxString posX = wxString::Format("%.3f", posArr[0] / 1000.0f);
    wxString posY = wxString::Format("%.3f", posArr[1] / 1000.0f);
    wxString posZ = wxString::Format("%.3f", posArr[2] / 1000.0f);

    auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
    if (pos == rowUuids.end())
      continue;

    int row = static_cast<int>(pos - rowUuids.begin());
    table->SetValue(
        wxVariant(posX), row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionX));
    table->SetValue(
        wxVariant(posY), row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionY));
    table->SetValue(
        wxVariant(posZ), row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionZ));
  }
}

// Writes normalized position values back into selected table rows.
void FixtureTablePanel::ApplyPositionValueUpdates(
    const std::vector<PositionValueUpdate> &updates) {
  if (!table)
    return;

  wxWindowUpdateLocker locker(table);
  for (const auto &update : updates) {
    auto pos = std::find(rowUuids.begin(), rowUuids.end(), update.uuid);
    if (pos == rowUuids.end())
      continue;

    int row = static_cast<int>(pos - rowUuids.begin());
    table->SetValue(
        wxVariant(wxString::FromUTF8(update.posX)), row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionX));
    table->SetValue(
        wxVariant(wxString::FromUTF8(update.posY)), row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionY));
    table->SetValue(
        wxVariant(wxString::FromUTF8(update.posZ)), row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionZ));
  }
}

// Copies a source row value into the same column for all selected rows.
void FixtureTablePanel::PropagateTypeValues(
    const wxDataViewItemArray &selections, int col) {
  if (!table)
    return;
  wxWindowUpdateLocker locker(table);
  FixtureTableEditService::PropagateTypeValues(table, selections, col);
}

// Persists edited table values back into scene fixtures and refreshes
// dependents.
void FixtureTablePanel::UpdateSceneData(
    bool logChanges, SceneDataUpdateType updateType,
    const std::vector<unsigned int> *targetRows) {

  ConfigManagerSceneAdapter adapter;
  std::unordered_set<std::string> changedWeightPositions;

  switch (updateType) {
  case SceneDataUpdateType::kPatchOnly:
    FixtureTableEditService::UpdatePatchForRows(adapter, table, rowUuids,
                                                logChanges);
    break;
  case SceneDataUpdateType::kAppearanceOnly:
    FixtureTableEditService::UpdateAppearanceForRows(adapter, table, rowUuids,
                                                     logChanges);
    break;
  case SceneDataUpdateType::kCategoryOnly:
    if (targetRows) {
      FixtureTableEditService::UpdateCategoryForRows(
          adapter, table, rowUuids, *targetRows, &manualCategoryUuidsPending,
          logChanges);
    } else {
      FixtureTableEditService::UpdateCategoryForRows(
          adapter, table, rowUuids, &manualCategoryUuidsPending, logChanges);
    }
    break;
  default:
    FixtureTableEditService::UpdateFullRowData(
        adapter, table, rowUuids, gdtfPaths, &manualCategoryUuidsPending,
        &changedWeightPositions, logChanges);
    break;
  }
  manualCategoryUuidsPending.clear();

  HoistLoadRecalculationPrompt::PromptAndApply(
      guiConfigServices->LegacyConfigManager(), this, changedWeightPositions);

  if (updateType != SceneDataUpdateType::kVisualLabelOnly &&
      updateType != SceneDataUpdateType::kCategoryOnly)
    RunValidationHighlights(updateType);

  if (RequiresRiggingRefresh(updateType) && RiggingPanel::Instance())
    RiggingPanel::Instance()->RefreshData();

  if (SummaryPanel::Instance() && IsActivePage() &&
      ShouldRefreshFixtureSummary(updateType))
    SummaryPanel::Instance()->ShowFixtureSummary();
}

// Applies a GDTF mode selection and fills dependent fixture attributes.
void FixtureTablePanel::ApplyModeForGdtf(const wxString &path,
                                         const wxString &preferredMode) {
  if (path.empty())
    return;

  std::vector<std::string> modes = GetGdtfModes(std::string(path.ToUTF8()));
  if (modes.empty())
    return;

  auto toLower = [](const std::string &s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
  };

  for (size_t i = 0; i < gdtfPaths.size() && i < (size_t)table->GetItemCount();
       ++i) {
    if (gdtfPaths[i] != path)
      continue;

    wxVariant v;
    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Mode));
    wxString currWx = v.GetString();
    std::string curr = std::string(currWx.ToUTF8());

    std::string chosen;
    if (!preferredMode.empty()) {
      std::string pref = std::string(preferredMode.ToUTF8());
      if (std::find(modes.begin(), modes.end(), pref) != modes.end())
        chosen = pref;
    }

    if (chosen.empty()) {
      chosen = curr;
      bool found = std::find(modes.begin(), modes.end(), curr) != modes.end();
      if (!found) {
        for (const std::string &m : modes) {
          std::string low = toLower(m);
          if (low == "default" || low == "standard") {
            chosen = m;
            found = true;
            break;
          }
        }
        if (!found)
          chosen = modes.front();
      }
    }

    if (chosen != curr)
      table->SetValue(
          wxVariant(wxString::FromUTF8(chosen)), i,
          FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Mode));

    int chCount = GetGdtfModeChannelCount(std::string(path.ToUTF8()), chosen);
    wxString chStr =
        chCount >= 0 ? wxString::Format("%d", chCount) : wxString();
    table->SetValue(wxVariant(chStr), i,
                    FixtureTableColumns::ToIndex(
                        FixtureTableColumns::Column::ChannelCount));
  }
}

// Highlights rows that share duplicate fixture IDs.
void FixtureTablePanel::HighlightDuplicateFixtureIds() {
  // Clear existing text colour highlights
  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    store->ClearRowTextColour(i);
    store->ClearCellTextColour(
        i, FixtureTableColumns::ToIndex(
               FixtureTableColumns::Column::FixtureId)); // Fixture ID column
  }

  std::unordered_map<long, std::vector<unsigned>> idRows;
  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    wxVariant v;
    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(
            FixtureTableColumns::Column::FixtureId)); // Fixture ID column
    long id = v.GetLong();
    idRows[id].push_back(i);
  }

  for (const auto &it : idRows) {
    if (it.second.size() > 1) {
      for (unsigned r : it.second)
        store->SetCellTextColour(r, 0, *wxRED);
    }
  }
}

// Runs validation highlight passes that correspond to the update scope.
void FixtureTablePanel::RunValidationHighlights(
    SceneDataUpdateType updateType) {
  switch (updateType) {
  case SceneDataUpdateType::kPatchOnly:
    HighlightPatchConflicts();
    break;
  case SceneDataUpdateType::kCategoryOnly:
    HighlightAutoFallbackCategories();
    break;
  case SceneDataUpdateType::kFixtureIdOnly:
    HighlightDuplicateFixtureIds();
    break;
  case SceneDataUpdateType::kVisualLabelOnly:
    return;
  case SceneDataUpdateType::kAppearanceOnly:
  case SceneDataUpdateType::kTransformOnly:
  case SceneDataUpdateType::kWeightOrPosition:
  case SceneDataUpdateType::kMetadataOnly:
  case SceneDataUpdateType::kGeneral:
    HighlightDuplicateFixtureIds();
    HighlightPatchConflicts();
    HighlightAutoFallbackCategories();
    break;
  }

  table->Refresh();
}

// Highlights fixtures whose categories come from automatic fallback.
void FixtureTablePanel::HighlightAutoFallbackCategories() {
  auto &fixtures = guiConfigServices->LegacyConfigManager().GetScene().fixtures;
  for (unsigned i = 0; i < table->GetItemCount(); ++i) {
    store->ClearCellTextColour(
        i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Category));

    if (i >= rowUuids.size())
      continue;
    auto it = fixtures.find(rowUuids[i]);
    if (it == fixtures.end())
      continue;

    const Fixture &fixture = it->second;
    if (fixture.categorySource == GdtfFixtureCategory::kAutoFallbackSource) {
      store->SetCellTextColour(i, 18, *wxRED);
    }
  }
}

// Reconciles displayed rows with current scene fixture ordering and keys.
void FixtureTablePanel::ResyncRows(
    const std::vector<std::string> &oldOrder,
    const std::vector<std::string> &selectedUuids,
    const std::vector<wxString> *oldPaths) {
  (void)oldOrder;
  (void)oldPaths;
  RebuildRowCachesFromRowKeys();
  selectionOrder.clear();
  selectionOrder.reserve(selectedUuids.size());
  for (const auto &uuid : selectedUuids) {
    auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
    if (pos != rowUuids.end())
      selectionOrder.push_back(static_cast<int>(pos - rowUuids.begin()));
  }

  {
    wxEventBlocker selectionBlocker(table, wxEVT_DATAVIEW_SELECTION_CHANGED);
    table->UnselectAll();
    for (const auto &uuid : selectedUuids) {
      auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
      if (pos != rowUuids.end())
        table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
    }
  }
  UpdateSelectionHighlight();
}

// Rebuilds internal row-to-UUID caches from current row keys.
void FixtureTablePanel::RebuildRowCachesFromRowKeys() {
  if (!table || !store)
    return;

  const unsigned int count = table->GetItemCount();
  rowUuids.assign(count, std::string());
  gdtfPaths.assign(count, wxString());
  for (unsigned int row = 0; row < count; ++row) {
    wxDataViewItem item = table->RowToItem(row);
    const wxUIntPtr rowKey = store->GetItemData(item);
    auto uuidIt = rowUuidByKey.find(rowKey);
    if (uuidIt != rowUuidByKey.end())
      rowUuids[row] = uuidIt->second;
    auto pathIt = gdtfPathByKey.find(rowKey);
    if (pathIt != gdtfPathByKey.end())
      gdtfPaths[row] = pathIt->second;
  }
}

// Resolves a fixture UUID from a data-view item selection handle.
std::string FixtureTablePanel::UuidForItem(const wxDataViewItem &item) const {
  if (!store || !item.IsOk())
    return {};
  const wxUIntPtr rowKey = store->GetItemData(item);
  auto it = rowUuidByKey.find(rowKey);
  if (it == rowUuidByKey.end())
    return {};
  return it->second;
}

// Updates stored GDTF path text and display label for a table row.
void FixtureTablePanel::SetGdtfPathForRow(unsigned int row,
                                          const wxString &path) {
  if (!table || !store)
    return;
  if (row >= table->GetItemCount())
    return;

  if (row >= gdtfPaths.size())
    gdtfPaths.resize(table->GetItemCount());
  gdtfPaths[row] = path;

  wxDataViewItem item = table->RowToItem(row);
  const wxUIntPtr rowKey = store->GetItemData(item);
  gdtfPathByKey[rowKey] = path;
}

// Rebuilds row caches after user-driven column sorting changes row order.
void FixtureTablePanel::OnColumnSorted(wxDataViewEvent &event) {
  RebuildRowCachesFromRowKeys();
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<std::string> selectedUuids;
  for (const auto &it : selections) {
    const std::string uuid = UuidForItem(it);
    if (!uuid.empty())
      selectedUuids.push_back(uuid);
  }
  std::vector<std::string> oldOrder = rowUuids;
  ResyncRows(oldOrder, selectedUuids);
  event.Skip();
}
