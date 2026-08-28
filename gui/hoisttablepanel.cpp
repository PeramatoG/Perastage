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
#include "hoisttablepanel.h"
#include "dataview_deferred_selection_guard.h"
#include "localized_unit_labels.h"

#include "colorfulrenderers.h"
#include "columnutils.h"
#include "configmanager.h"
#include "dataview_edit_commit.h"
#include "dummyprofilelibrary.h"
#include "editable_focus_utils.h"
#include "guiconfigservices.h"
#include "hang_position_dialog.h"
#include "hoist_load_recalculation_prompt.h"
#include "hoist_weight_distribution.h"
#include "layerpanel.h"
#include "matrixutils.h"
#include "scene_grouping.h"
#include "scene_node_operations.h"
#include "scene_view_refresh.h"
#include "riggingpanel.h"
#include "rigging_extra_weight_settings.h"
#include "selection_origin_token.h"
#include "stringutils.h"
#include "summarypanel.h"
#include "support.h"
#include "table_column_indices.h"
#include "units/unit_label_utils.h"
#include "units/units.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <optional>
#include <wx/aui/aui.h>
#include <wx/choicdlg.h>
#include <wx/notebook.h>
#include <wx/version.h>
#include <wx/wupdlock.h> // freeze/thaw UI during batch edits

static HoistTablePanel *s_instance = nullptr;

namespace {

using HoistColumn = HoistTableColumns::Column;

// Converts a hoist column to its stable model index.
constexpr int ColumnIndex(HoistColumn column) {
  return TableColumnIndices::ToIndex(column);
}

// Checks whether a hoist column contains numeric data.
bool IsNumericColumn(HoistColumn column) {
  return column >= HoistColumn::PositionX && column <= HoistColumn::Load;
}

// Checks whether a hoist column contains rotation data.
bool IsRotationColumn(HoistColumn column) {
  return column >= HoistColumn::Roll && column <= HoistColumn::Yaw;
}

const wxString &DegreeSymbol() {
  static const wxString kDegreeSymbol = wxString::FromUTF8("\xC2\xB0");
  return kDegreeSymbol;
}

constexpr const char *kUnassignedPosition = "Unassigned";

std::string NormalizePositionName(const std::string &positionName) {
  return positionName.empty() ? kUnassignedPosition : positionName;
}

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

// Reports whether a hoist load is an explicit user-entered value.
bool HasManualLoad(const Support &support) {
  return support.loadSource == "Manual";
}

// Reports whether an automatic load depends on missing scene weight data.
bool HasMissingLoadInputs(const MvrScene &scene, const Support &support) {
  const std::string position = NormalizePositionName(support.positionName);
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (NormalizePositionName(fixture.positionName) == position &&
        fixture.weightKg <= 0.0f)
      return true;
  }
  for (const auto &[uuid, truss] : scene.trusses) {
    if (NormalizePositionName(truss.positionName) == position &&
        truss.weightKg <= 0.0f)
      return true;
  }
  for (const auto &[uuid, candidate] : scene.supports) {
    if (NormalizePositionName(candidate.positionName) != position)
      continue;
    const auto effective = ResolveEffectiveSupportData(candidate);
    if (effective.weightKg <= 0.0f)
      return true;
  }
  return false;
}

// Recalculates selected automatic hoist loads from current rigging totals.
void RecalculateAutomaticLoads(
    ConfigManager &cfg, const std::vector<std::string> &supportUuids) {
  MvrScene &scene = cfg.GetScene();
  const auto extraWeights = RiggingExtraWeightSettings::ParseEntries(
      cfg.GetValue(RiggingExtraWeightSettings::ConfigKey()));
  const auto roundedTotals =
      HoistWeightDistribution::BuildRoundedRiggingTotalByHangPosition(
          scene,
          RiggingExtraWeightSettings::BuildKilogramsByPosition(extraWeights));
  HoistWeightDistribution::ApplyForImportedSupports(scene, supportUuids,
                                                     roundedTotals);
}

// Calculates automatic load values without changing the active scene.
std::unordered_map<std::string, float> CalculateAutomaticLoads(
    const ConfigManager &cfg, const std::vector<std::string> &supportUuids) {
  MvrScene scene = cfg.GetScene();
  for (const std::string &uuid : supportUuids) {
    auto supportIt = scene.supports.find(uuid);
    if (supportIt != scene.supports.end())
      supportIt->second.loadSource = "Auto";
  }
  const auto extraWeights = RiggingExtraWeightSettings::ParseEntries(
      cfg.GetValue(RiggingExtraWeightSettings::ConfigKey()));
  const auto roundedTotals =
      HoistWeightDistribution::BuildRoundedRiggingTotalByHangPosition(
          scene,
          RiggingExtraWeightSettings::BuildKilogramsByPosition(extraWeights));
  HoistWeightDistribution::ApplyForImportedSupports(scene, supportUuids,
                                                     roundedTotals);

  std::unordered_map<std::string, float> result;
  for (const std::string &uuid : supportUuids) {
    auto supportIt = scene.supports.find(uuid);
    if (supportIt != scene.supports.end())
      result[uuid] = supportIt->second.loadKg;
  }
  return result;
}

void SetTableAndChildTooltips(wxDataViewListCtrl *table,
                              const wxString &tooltip) {
  if (!table)
    return;

  table->SetToolTip(tooltip);
  wxWindowList &children = table->GetChildren();
  for (wxWindowList::compatibility_iterator it = children.GetFirst(); it;
       it = it->GetNext()) {
    if (wxWindow *child = it->GetData())
      child->SetToolTip(tooltip);
  }
}

wxPoint NormalizeMousePositionForTable(wxDataViewListCtrl *table,
                                       const wxMouseEvent &event) {
  wxPoint position = event.GetPosition();
  wxWindow *sourceWindow = dynamic_cast<wxWindow *>(event.GetEventObject());
  if (!table || !sourceWindow || sourceWindow == table)
    return position;

  return table->ScreenToClient(sourceWindow->ClientToScreen(position));
}

template <typename Owner>
void BindTableHoverEvents(wxDataViewListCtrl *table, Owner *owner,
                          void (Owner::*onMouseMove)(wxMouseEvent &),
                          void (Owner::*onMouseLeave)(wxMouseEvent &)) {
  if (!table || !owner)
    return;

  auto bindEvents = [&](wxWindow *window) {
    if (!window)
      return;
    window->Unbind(wxEVT_MOTION, onMouseMove, owner);
    window->Unbind(wxEVT_LEAVE_WINDOW, onMouseLeave, owner);
    window->Bind(wxEVT_MOTION, onMouseMove, owner);
    window->Bind(wxEVT_LEAVE_WINDOW, onMouseLeave, owner);
  };

  bindEvents(table);
  wxWindowList &children = table->GetChildren();
  for (wxWindowList::compatibility_iterator it = children.GetFirst(); it;
       it = it->GetNext()) {
    bindEvents(it->GetData());
  }
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

class LoadEditDialog final : public wxDialog {
public:
  // Builds a load editor that can insert the calculated automatic value.
  LoadEditDialog(wxWindow *parent, const wxString &title,
                 const wxString &currentValue,
                 const wxString &automaticValue)
      : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        automaticValue(automaticValue) {
    auto *rootSizer = new wxBoxSizer(wxVERTICAL);
    valueCtrl = new wxTextCtrl(this, wxID_ANY, currentValue);
    rootSizer->Add(valueCtrl, 0, wxEXPAND | wxALL, 10);

    auto *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    auto *recalculateButton =
        new wxButton(this, wxID_ANY, _("Use automatic value"));
    buttonSizer->Add(recalculateButton, 0, wxRIGHT, 8);
    buttonSizer->AddStretchSpacer();
    auto *okButton = new wxButton(this, wxID_OK);
    buttonSizer->Add(okButton, 0, wxRIGHT, 8);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL), 0);
    rootSizer->Add(buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    recalculateButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
      valueCtrl->SetValue(this->automaticValue);
      valueCtrl->SetFocus();
      valueCtrl->SelectAll();
    });
    Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
      EndModal(wxID_OK);
    }, wxID_OK);

    SetSizerAndFit(rootSizer);
    okButton->SetDefault();
    valueCtrl->SetFocus();
    valueCtrl->SelectAll();
  }

  // Returns the edited load text.
  wxString GetValue() const { return valueCtrl->GetValue(); }

private:
  wxTextCtrl *valueCtrl = nullptr;
  wxString automaticValue;
};

bool IsNumChar(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
         c == '+';
}

inline bool NearlyEqualFloat(float a, float b) {
  return std::abs(a - b) < 0.0001f;
}

void MarkTextFieldManualIfEdited(const std::string &editedValue,
                                 const std::string &oldEffectiveValue,
                                 std::string &fieldSource,
                                 const std::string &oldFieldValue,
                                 std::string &fieldValue) {
  if (editedValue != oldEffectiveValue) {
    fieldSource = "Manual";
    fieldValue = editedValue;
    return;
  }

  if (!IsManualHoistDataSource(fieldSource))
    fieldValue = oldFieldValue;
}

void MarkNumericFieldManualIfEdited(float editedValue, float oldEffectiveValue,
                                    std::string &fieldSource,
                                    float oldFieldValue, float &fieldValue) {
  if (!NearlyEqualFloat(editedValue, oldEffectiveValue)) {
    fieldSource = "Manual";
    fieldValue = editedValue;
    return;
  }

  if (!IsManualHoistDataSource(fieldSource))
    fieldValue = oldFieldValue;
}

std::optional<HoistPresetDefaults> FindPresetDefaults(const Support &support) {
  std::optional<DummyHoistProfile> profile;
  if (!support.dummyProfileId.empty())
    profile = DummyProfileLibrary::FindById(support.dummyProfileId);
  if (!profile.has_value() && !support.dummyPreset.empty())
    profile = DummyProfileLibrary::FindByDisplayName(support.dummyPreset);
  if (!profile.has_value())
    return std::nullopt;

  HoistPresetDefaults defaults;
  defaults.motorName = profile->motorName;
  defaults.motorManufacturer = profile->motorManufacturer;
  defaults.motorModel = profile->motorModel;
  defaults.capacityKg = profile->capacityKg;
  defaults.weightKg = profile->weightKg;
  defaults.hoistFunction = profile->hoistFunction;
  return defaults;
}

std::optional<HoistFixtureDefaults>
FindFixtureDefaults(const MvrScene &scene, const Support &support) {
  if (support.motorFixtureUuid.empty())
    return std::nullopt;
  auto it = scene.fixtures.find(support.motorFixtureUuid);
  if (it == scene.fixtures.end())
    return std::nullopt;
  return BuildHoistFixtureDefaults(it->second);
}

wxArrayString BuildDummyPresetChoices() {
  wxArrayString choices;
  choices.push_back("");
  const auto profiles = DummyProfileLibrary::LoadProfiles();
  for (const auto &profile : profiles)
    choices.push_back(wxString::FromUTF8(profile.displayName));
  return choices;
}

std::string ResolveDummyProfileDisplayName(const Support &support) {
  if (!support.dummyProfileId.empty()) {
    const auto profile = DummyProfileLibrary::FindById(support.dummyProfileId);
    if (profile.has_value())
      return profile->displayName;
  }
  return support.dummyPreset;
}

RangeParts SplitRangeParts(const wxString &value) {
  std::string lower = value.Lower().ToStdString();
  std::string normalized;
  normalized.reserve(lower.size() + 4);
  bool usedSeparator = false;
  bool trailingSeparator = false;
  for (size_t i = 0; i < lower.size();) {
    if (lower.compare(i, 4, "thru") == 0) {
      normalized.push_back(' ');
      usedSeparator = true;
      trailingSeparator = true;
      i += 4;
      continue;
    }
    if (lower[i] == 't') {
      char prev = (i > 0) ? lower[i - 1] : '\0';
      char next = (i + 1 < lower.size()) ? lower[i + 1] : '\0';
      bool standalone =
          (i == 0 || std::isspace(static_cast<unsigned char>(prev))) &&
          (i + 1 >= lower.size() ||
           std::isspace(static_cast<unsigned char>(next)));
      if (standalone || IsNumChar(prev) || IsNumChar(next)) {
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
  for (const auto &part : rawParts)
    if (!part.IsEmpty())
      parts.push_back(part);
  return {parts, usedSeparator, trailingSeparator};
}
} // namespace

// Initializes the hoist table controls and optionally populates scene rows.
HoistTablePanel::HoistTablePanel(
    wxWindow *parent, IGuiConfigServices *services,
    gui::InitialPopulationPolicy populationPolicy)
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
  table->Bind(wxEVT_LEFT_DOWN, &HoistTablePanel::OnLeftDown, this);
  table->Bind(wxEVT_LEFT_UP, &HoistTablePanel::OnLeftUp, this);
  BindTableHoverEvents(table, this, &HoistTablePanel::OnMouseMove,
                       &HoistTablePanel::OnMouseLeave);
  table->CallAfter([this]() {
    BindTableHoverEvents(table, this, &HoistTablePanel::OnMouseMove,
                         &HoistTablePanel::OnMouseLeave);
  });
  table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
              &HoistTablePanel::OnSelectionChanged, this);

  table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &HoistTablePanel::OnContextMenu,
              this);
  table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED, &HoistTablePanel::OnColumnSorted,
              this);

  Bind(wxEVT_MOUSE_CAPTURE_LOST, &HoistTablePanel::OnCaptureLost, this);

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
  if (populationPolicy == gui::InitialPopulationPolicy::Immediate)
    ReloadData();

  sizer->Add(table, 1, wxEXPAND | wxALL, 5);
  SetSizer(sizer);
}

// Releases table resources and detaches the hoist pane from AUI layout
// management.
HoistTablePanel::~HoistTablePanel() {
  if (wxAuiManager *manager = wxAuiManager::GetManager(this))
    manager->DetachPane(this);
  if (s_instance == this)
    s_instance = nullptr;
  store = nullptr;
}

void HoistTablePanel::InitializeTable() {
  const auto distanceUnit = ResolveDistanceUnitSystem();
  const auto weightUnit = ResolveWeightUnitSystem();
  const wxString distanceSuffix =
      wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnit));
  const wxString weightSuffix =
      wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
  columnLabels = {
      _("Hoist ID"),
      _("Name"),
      _("Type"),
      _("Function"),
      _("Motor"),
      _("Dummy Preset"),
      _("Layer"),
      _("Hang Pos"),
      ui::LocalizedLabelWithUnit(_("Pos X"), distanceSuffix),
      ui::LocalizedLabelWithUnit(_("Pos Y"), distanceSuffix),
      ui::LocalizedLabelWithUnit(_("Pos Z"), distanceSuffix),
      _("Roll (X)"),
      _("Pitch (Y)"),
      _("Yaw (Z)"),
      ui::LocalizedLabelWithUnit(_("Chain Length"), distanceSuffix),
      ui::LocalizedLabelWithUnit(_("Capacity"), weightSuffix),
      ui::LocalizedLabelWithUnit(_("Weight"), weightSuffix),
      ui::LocalizedLabelWithUnit(_("Load"), weightSuffix)};
  std::vector<int> widths = {70, 150, 120, 120, 130, 150, 100, 120, 80,
                             80, 80,  80,  80,  80,  110, 110, 100, 100};
  if (columnLabels.size() != TableColumnIndices::Count<HoistColumn>() ||
      widths.size() != TableColumnIndices::Count<HoistColumn>())
    return;
  for (size_t i = 0; i < columnLabels.size(); ++i)
    table->AppendColumn(new wxDataViewColumn(
        columnLabels[i],
        new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT, wxALIGN_LEFT), i,
        widths[i], wxALIGN_LEFT,
        wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
  ColumnUtils::EnforceMinColumnWidth(table);
}

void HoistTablePanel::ReloadData() {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContentChanged();
  const auto distanceUnit = ResolveDistanceUnitSystem();
  const auto weightUnit = ResolveWeightUnitSystem();
  const wxString distanceSuffix =
      wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnit));
  const wxString weightSuffix =
      wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
  columnLabels[ColumnIndex(HoistColumn::PositionX)] = ui::LocalizedLabelWithUnit(_("Pos X"), distanceSuffix);
  columnLabels[ColumnIndex(HoistColumn::PositionY)] = ui::LocalizedLabelWithUnit(_("Pos Y"), distanceSuffix);
  columnLabels[ColumnIndex(HoistColumn::PositionZ)] = ui::LocalizedLabelWithUnit(_("Pos Z"), distanceSuffix);
  columnLabels[ColumnIndex(HoistColumn::ChainLength)] =
      ui::LocalizedLabelWithUnit(_("Chain Length"), distanceSuffix);
  columnLabels[ColumnIndex(HoistColumn::Capacity)] = ui::LocalizedLabelWithUnit(_("Capacity"), weightSuffix);
  columnLabels[ColumnIndex(HoistColumn::Weight)] = ui::LocalizedLabelWithUnit(_("Weight"), weightSuffix);
  columnLabels[ColumnIndex(HoistColumn::Load)] = ui::LocalizedLabelWithUnit(_("Load"), weightSuffix);
  for (size_t i = 0; i < columnLabels.size(); ++i) {
    if (auto *column = table->GetColumn(static_cast<unsigned int>(i)))
      column->SetTitle(columnLabels[i]);
  }

  table->DeleteAllItems();
  rowUuids.clear();
  rowLoadStates.clear();
  rowUuidByKey.clear();
  loadStateByKey.clear();
  nextRowKey = 1;
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  MvrScene &scene = cfg.GetScene();
  auto &supports = scene.supports;
  std::vector<std::string> missingAutomaticLoadUuids;
  for (const auto &[uuid, support] : supports) {
    if (!HasManualLoad(support) && support.loadKg <= 0.0f)
      missingAutomaticLoadUuids.push_back(uuid);
  }
  if (!missingAutomaticLoadUuids.empty())
    RecalculateAutomaticLoads(cfg, missingAutomaticLoadUuids);
  const auto extraWeights = RiggingExtraWeightSettings::ParseEntries(
      cfg.GetValue(
          RiggingExtraWeightSettings::ConfigKey()));

  std::vector<std::pair<std::string, Support *>> sorted;
  sorted.reserve(supports.size());
  for (auto &[uuid, support] : supports)
    sorted.emplace_back(uuid, &support);

  std::sort(sorted.begin(), sorted.end(), [](const auto &A, const auto &B) {
    const Support *a = A.second;
    const Support *b = B.second;
    if (a->layer != b->layer)
      return StringUtils::NaturalLess(a->layer, b->layer);
    if (a->positionName != b->positionName)
      return StringUtils::NaturalLess(a->positionName, b->positionName);
    return StringUtils::NaturalLess(a->name, b->name);
  });

  int hoistId = 1;
  for (const auto &pair : sorted) {
    const std::string &uuid = pair.first;
    Support &support = *pair.second;
    wxVector<wxVariant> row;

    row.push_back(wxVariant(hoistId));
    wxString name = wxString::FromUTF8(support.name);
    wxString type = wxString::FromUTF8(support.function);
    support.hoistDataSource = NormalizeHoistDataSource(support.hoistDataSource);
    support.motorNameSource = ResolveHoistFieldDataSource(
        support.motorNameSource, support.hoistDataSource);
    support.motorManufacturerSource = ResolveHoistFieldDataSource(
        support.motorManufacturerSource, support.hoistDataSource);
    support.motorModelSource = ResolveHoistFieldDataSource(
        support.motorModelSource, support.hoistDataSource);
    support.capacitySource = ResolveHoistFieldDataSource(
        support.capacitySource, support.hoistDataSource);
    support.weightSource = ResolveHoistFieldDataSource(support.weightSource,
                                                       support.hoistDataSource);
    support.hoistFunctionSource = ResolveHoistFieldDataSource(
        support.hoistFunctionSource, support.hoistDataSource);
    const auto effective =
        ResolveEffectiveSupportData(support, FindPresetDefaults(support),
                                    FindFixtureDefaults(scene, support));
    support.hoistFunction = NormalizeHoistFunction(support.hoistFunction);
    wxString hoistFunction = wxString::FromUTF8(effective.hoistFunction);
    wxString motorName = wxString::FromUTF8(effective.motorName);
    wxString dummyPreset =
        wxString::FromUTF8(ResolveDummyProfileDisplayName(support));
    wxString layer = support.layer == DEFAULT_LAYER_NAME
                         ? wxString()
                         : wxString::FromUTF8(support.layer);
    wxString posName = wxString::FromUTF8(support.positionName);

    auto posArr = support.transform.o;
    wxString posX = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        posArr[0], distanceUnit, Units::ValueFormatContext::Table));
    wxString posY = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        posArr[1], distanceUnit, Units::ValueFormatContext::Table));
    wxString posZ = wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        posArr[2], distanceUnit, Units::ValueFormatContext::Table));

    auto euler = MatrixUtils::MatrixToEuler(support.transform);
    wxString roll = wxString::Format("%.1f", euler[2]) + DegreeSymbol();
    wxString pitch = wxString::Format("%.1f", euler[1]) + DegreeSymbol();
    wxString yaw = wxString::Format("%.1f", euler[0]) + DegreeSymbol();

    wxString chainLen = wxString::Format("%.2f", support.chainLength);
    wxString capacity = wxString::FromUTF8(Units::FormatWeightFromKilograms(
        effective.capacityKg, weightUnit, Units::ValueFormatContext::Table));
    wxString weight = wxString::FromUTF8(Units::FormatWeightFromKilograms(
        effective.weightKg, weightUnit, Units::ValueFormatContext::Table));
    wxString load = wxString::FromUTF8(Units::FormatWeightFromKilograms(
        support.loadKg, weightUnit, Units::ValueFormatContext::Table));
    const auto loadState =
        HoistLoadLimitUtils::Evaluate(support.loadKg, effective.capacityKg);

    row.push_back(name);
    row.push_back(type);
    row.push_back(hoistFunction);
    row.push_back(motorName);
    row.push_back(dummyPreset);
    row.push_back(layer);
    row.push_back(posName);
    row.push_back(posX);
    row.push_back(posY);
    row.push_back(posZ);
    row.push_back(roll);
    row.push_back(pitch);
    row.push_back(yaw);
    row.push_back(chainLen);
    row.push_back(capacity);
    row.push_back(weight);
    row.push_back(load);

    const wxUIntPtr rowKey = nextRowKey++;
    store->AppendItem(row, rowKey);
    const unsigned int rowIndex = static_cast<unsigned int>(rowUuids.size());
    const bool manualLoad = HasManualLoad(support);
    const auto extraWeightIt =
        extraWeights.find(NormalizePositionName(support.positionName));
    const bool unvalidatedExtraWeight =
        extraWeightIt != extraWeights.end() &&
        extraWeightIt->second.requiresValidation;
    const bool missingLoadInputs = !manualLoad &&
        (HasMissingLoadInputs(scene, support) || unvalidatedExtraWeight);
    if (HoistLoadLimitUtils::IsCritical(loadState) || manualLoad ||
        missingLoadInputs)
      store->SetCellTextColour(rowIndex, ColumnIndex(HoistColumn::Load),
                               *wxRED);
    else
      store->ClearCellTextColour(rowIndex, ColumnIndex(HoistColumn::Load));
    rowUuids.push_back(uuid);
    rowLoadStates.push_back(loadState);
    rowUuidByKey[rowKey] = uuid;
    loadStateByKey[rowKey] = loadState;
    ++hoistId;
  }

  if (LayerPanel::Instance())
    LayerPanel::Instance()->ReloadLayers();
  if (SummaryPanel::Instance())
    SummaryPanel::Instance()->ShowHoistSummary();
  if (RiggingPanel::Instance())
    RiggingPanel::Instance()->RefreshData();
}

void HoistTablePanel::OnContextMenu(wxDataViewEvent &event) {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyItemActivated(event.GetItem(), event.GetColumn());
  wxDataViewItem item = event.GetItem();
  int col = event.GetColumn();
  const auto namedColumn = TableColumnIndices::FromIndex<HoistColumn>(col);
  if (!item.IsOk() || !namedColumn ||
      static_cast<size_t>(col) >= columnLabels.size())
    return;

  // Freeze UI updates while performing bulk table modifications. Without
  // freezing, wxDataViewListCtrl repaints after each SetValue call and resort
  // operation, which can cause noticeable lag when editing many rows at once.
  // wxWindowUpdateLocker automatically calls Freeze() on the given window and
  // Thaw() when it goes out of scope, ensuring that the table redraw only once
  // after all modifications are complete.
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
  std::unordered_map<std::string, float> editedAutomaticLoads;
  if (*namedColumn == HoistColumn::Function) {
    wxArrayString choices;
    for (const auto &option : GetHoistFunctionOptions())
      choices.push_back(wxString::FromUTF8(option));
    choices.push_back(_("Other..."));

    wxSingleChoiceDialog sdlg(this, _("Select function"), _("Function"), choices);
    // With the following code:
    int selIdx = choices.Index(current.GetString());
    if (selIdx != wxNOT_FOUND)
        sdlg.SetSelection(selIdx);
    else
        sdlg.SetSelection(static_cast<int>(choices.size() - 1));
    if (sdlg.ShowModal() != wxID_OK)
      return;
    wxString sel = sdlg.GetStringSelection();
    if (sel == "Other...") {
      wxTextEntryDialog otherDlg(this, _("Enter function"), _("Function"),
                                 current.GetString());
      if (otherDlg.ShowModal() != wxID_OK)
        return;
      sel = otherDlg.GetValue().Trim(true).Trim(false);
      if (sel.empty())
        return;
    }
    for (const auto &itSel : selections) {
      int r = table->ItemToRow(itSel);
      if (r != wxNOT_FOUND)
        table->SetValue(wxVariant(sel), r, col);
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

  if (*namedColumn == HoistColumn::DummyPreset) {
    ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
    const auto &supports = cfg.GetScene().supports;

    wxArrayString choices = BuildDummyPresetChoices();
    wxSingleChoiceDialog sdlg(this, _("Select dummy preset"), _("Dummy Preset"),
                              choices);
    if (sdlg.ShowModal() != wxID_OK)
      return;

    wxString sel = sdlg.GetStringSelection();
    bool updatedAnyRow = false;
    for (const auto &itSel : selections) {
      int r = table->ItemToRow(itSel);
      if (r == wxNOT_FOUND || static_cast<size_t>(r) >= rowUuids.size())
        continue;

      auto supportIt = supports.find(rowUuids[static_cast<size_t>(r)]);
      if (supportIt == supports.end())
        continue;
      if (!supportIt->second.motorFixtureUuid.empty())
        continue;

      table->SetValue(wxVariant(sel), r, col);
      updatedAnyRow = true;
    }

    if (!updatedAnyRow) {
      wxMessageBox(_("Dummy preset can only be assigned when there is no linked motor fixture."),
                   _("Dummy Preset"), wxOK | wxICON_INFORMATION, this);
      return;
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

  if (*namedColumn == HoistColumn::Layer) {
    auto layers = guiConfigServices->LegacyConfigManager().GetLayerNames();
    wxArrayString choices;
    for (const auto &n : layers)
      choices.push_back(wxString::FromUTF8(n));
    wxSingleChoiceDialog sdlg(this, _("Select layer"), _("Layer"), choices);
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

  if (*namedColumn == HoistColumn::Load) {
    const auto automaticLoads =
        CalculateAutomaticLoads(guiConfigServices->LegacyConfigManager(),
                                selectedUuids);
    wxString automaticValue = current.GetString();
    const std::string editedUuid = UuidForItem(item);
    auto automaticIt = automaticLoads.find(editedUuid);
    if (automaticIt != automaticLoads.end()) {
      automaticValue = wxString::FromUTF8(Units::FormatWeightFromKilograms(
          automaticIt->second, ResolveWeightUnitSystem(),
          Units::ValueFormatContext::Table));
    }
    LoadEditDialog loadDialog(this, columnLabels[col], current.GetString(),
                              automaticValue);
    if (loadDialog.ShowModal() != wxID_OK)
      return;

    current = wxVariant(loadDialog.GetValue());
    editedAutomaticLoads = automaticLoads;
  }

  wxString value;
  if (*namedColumn == HoistColumn::Load) {
    value = current.GetString().Trim(true).Trim(false);
  } else if (*namedColumn == HoistColumn::HangPosition) {
    std::vector<unsigned int> selectedRows;
    for (const auto &it : selections) {
      const int row = table->ItemToRow(it);
      if (row != wxNOT_FOUND)
        selectedRows.push_back(static_cast<unsigned int>(row));
    }

    HangPositionDialogResult result;
    if (!ShowHangPositionDialog(this, current.GetString(), &result))
      return;
    ApplySharedHangPositionChanges(result, false, {}, false, {}, true,
                                    selectedRows);
    ResyncRows(oldOrder, selectedUuids);
    return;
  } else {
    wxTextEntryDialog dlg(this, _("Edit value:"), columnLabels[col],
                          current.GetString());
    if (dlg.ShowModal() != wxID_OK)
      return;
    value = dlg.GetValue().Trim(true).Trim(false);
  }

  const bool numericCol = IsNumericColumn(*namedColumn);
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
          out = wxString::Format(
              (*namedColumn >= HoistColumn::ChainLength) ? "%.2f" : "%.3f",
              newVal);
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
          out = wxString::Format(
              (*namedColumn >= HoistColumn::ChainLength) ? "%.2f" : "%.3f",
              val);

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

  ResyncRows(oldOrder, selectedUuids);
  pendingAutomaticLoadByUuid.insert(editedAutomaticLoads.begin(),
                                    editedAutomaticLoads.end());

  UpdateSceneData();
  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  } else if (Viewer2DPanel::Instance()) {
    Viewer2DPanel::Instance()->UpdateScene();
  }
}

void HoistTablePanel::OnLeftDown(wxMouseEvent &evt) {
  wxDataViewItem item;
  wxDataViewColumn *col;
  table->HitTest(evt.GetPosition(), item, col);
  startRow = table->ItemToRow(item);
  if (startRow != wxNOT_FOUND) {
    dragSelecting = true;
    CaptureMouse();
  }
  evt.Skip();
}

void HoistTablePanel::OnLeftUp(wxMouseEvent &evt) {
  if (dragSelecting) {
    dragSelecting = false;
    ReleaseMouse();
  }
  evt.Skip();
}

void HoistTablePanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(evt)) {
  dragSelecting = false;
}

void HoistTablePanel::OnMouseMove(wxMouseEvent &evt) {
  UpdateHoverTooltip(NormalizeMousePositionForTable(table, evt));

  if (!dragSelecting || !evt.Dragging()) {
    evt.Skip();
    return;
  }
  wxDataViewItem item;
  wxDataViewColumn *col;
  table->HitTest(NormalizeMousePositionForTable(table, evt), item, col);
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

void HoistTablePanel::OnMouseLeave(wxMouseEvent &evt) {
  if (!activeHoverTooltip.IsEmpty()) {
    SetTableAndChildTooltips(table, wxString());
    activeHoverTooltip.clear();
  }
  evt.Skip();
}

void HoistTablePanel::UpdateHoverTooltip(const wxPoint &position) {
  if (gui::IsEditableWidgetFocused(wxWindow::FindFocus()))
    return;

  wxDataViewItem item;
  wxDataViewColumn *column = nullptr;
  table->HitTest(position, item, column);

  wxString tooltip;
  if (item.IsOk() && column) {
    const int row = table->ItemToRow(item);
    const int modelColumn = column->GetModelColumn();
    if (modelColumn == ColumnIndex(HoistColumn::Load) &&
        IsRedCell(store, row, modelColumn) && row >= 0 &&
        static_cast<size_t>(row) < rowLoadStates.size()) {
      const std::string uuid = rowUuids[static_cast<size_t>(row)];
      const auto &scene = guiConfigServices->LegacyConfigManager().GetScene();
      auto supportIt = scene.supports.find(uuid);
      if (supportIt != scene.supports.end() &&
          HasManualLoad(supportIt->second)) {
        tooltip = "Load was entered manually.";
      } else if (supportIt != scene.supports.end() &&
                 HasMissingLoadInputs(scene, supportIt->second)) {
        tooltip = "Automatic load uses one or more missing weight values.";
      } else {
        tooltip = wxString::FromUTF8(HoistLoadLimitUtils::TooltipForState(
            rowLoadStates[static_cast<size_t>(row)]));
        if (tooltip.empty())
          tooltip = "Automatic load uses weight data that requires review.";
      }
    }
  }

  if (tooltip == activeHoverTooltip)
    return;

  SetTableAndChildTooltips(table, tooltip);
  activeHoverTooltip = tooltip;
}

// Handles table selection events and defers transient single-click collapses.
void HoistTablePanel::OnSelectionChanged(wxDataViewEvent &evt) {
  if (deferredSelectionGuard && deferredSelectionGuard->HandleSelectionChanged())
    return;

  SyncSelectionFromTable();
  evt.Skip();
}

// Synchronizes selected table rows with the shared scene selection state.
void HoistTablePanel::SyncSelectionFromTable() {
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
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  if (uuids != cfg.GetSelectedSupports()) {
    cfg.PushUndoState("support selection");
    cfg.SetSelectedSupports(uuids);
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

}

void HoistTablePanel::UpdateSelectionHighlight() {
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

void HoistTablePanel::ApplyPositionValueUpdates(
    const std::vector<PositionValueUpdate> &updates) {
  if (!table)
    return;

  wxWindowUpdateLocker locker(table);
  for (const auto &update : updates) {
    auto pos = std::find(rowUuids.begin(), rowUuids.end(), update.uuid);
    if (pos == rowUuids.end())
      continue;

    int row = static_cast<int>(pos - rowUuids.begin());
    table->SetValue(wxVariant(wxString::FromUTF8(update.posX)), row,
                    ColumnIndex(HoistColumn::PositionX));
    table->SetValue(wxVariant(wxString::FromUTF8(update.posY)), row,
                    ColumnIndex(HoistColumn::PositionY));
    table->SetValue(wxVariant(wxString::FromUTF8(update.posZ)), row,
                    ColumnIndex(HoistColumn::PositionZ));
  }
}

// Persists edited support table values back into scene supports.
void HoistTablePanel::UpdateSceneData(bool logChanges) {
  // Ensure in-place cell editors commit pending values before reading table
  // rows.
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);

  (void)logChanges;
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  auto &scene = cfg.GetScene();
  size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());
  bool anyChanged = false;
  std::unordered_set<std::string> changedWeightPositions;
  bool undoPushed = false;
  auto pushUndoIfNeeded = [&]() {
    if (!undoPushed) {
      cfg.PushUndoState("edit support");
      undoPushed = true;
    }
  };

  // Apply table values only when a support row actually changed.
  for (size_t i = 0; i < count; ++i) {
    auto it = scene.supports.find(rowUuids[i]);
    if (it == scene.supports.end())
      continue;

    const Support old = it->second;
    Support next = old;

    wxVariant v;
    table->GetValue(v, i, ColumnIndex(HoistColumn::Name));
    next.name = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, ColumnIndex(HoistColumn::Type));
    next.function = std::string(v.GetString().ToUTF8());

    const auto oldEffective = ResolveEffectiveSupportData(
        old, FindPresetDefaults(old), FindFixtureDefaults(scene, old));

    table->GetValue(v, i, ColumnIndex(HoistColumn::Function));
    const std::string editedHoistFunction =
        NormalizeHoistFunction(std::string(v.GetString().ToUTF8()));
    next.hoistFunction = editedHoistFunction;

    table->GetValue(v, i, ColumnIndex(HoistColumn::Motor));
    const std::string editedMotorName = std::string(v.GetString().ToUTF8());
    next.motorName = editedMotorName;

    table->GetValue(v, i, ColumnIndex(HoistColumn::DummyPreset));
    next.dummyPreset = std::string(v.GetString().ToUTF8());
    if (next.dummyPreset.empty()) {
      next.dummyProfileId.clear();
    } else {
      const auto profile =
          DummyProfileLibrary::FindByDisplayName(next.dummyPreset);
      next.dummyProfileId = profile.has_value() ? profile->id : "";
    }

    table->GetValue(v, i, ColumnIndex(HoistColumn::Layer));
    std::string layerStr = std::string(v.GetString().ToUTF8());
    if (layerStr.empty())
      next.layer.clear();
    else
      next.layer = layerStr;

    table->GetValue(v, i, ColumnIndex(HoistColumn::HangPosition));
    next.positionName = std::string(v.GetString().ToUTF8());

    const auto distanceUnit = ResolveDistanceUnitSystem();
    const auto weightUnit = ResolveWeightUnitSystem();
    double xMm = old.transform.o[0], yMm = old.transform.o[1],
           zMm = old.transform.o[2];
    table->GetValue(v, i, ColumnIndex(HoistColumn::PositionX));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnit);
        parsed.has_value())
      xMm = *parsed;
    table->GetValue(v, i, ColumnIndex(HoistColumn::PositionY));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnit);
        parsed.has_value())
      yMm = *parsed;
    table->GetValue(v, i, ColumnIndex(HoistColumn::PositionZ));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnit);
        parsed.has_value())
      zMm = *parsed;

    double roll = 0, pitch = 0, yaw = 0;
    table->GetValue(v, i, ColumnIndex(HoistColumn::Roll));
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
      s.ToDouble(&roll);
    }
    table->GetValue(v, i, ColumnIndex(HoistColumn::Pitch));
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
      s.ToDouble(&pitch);
    }
    table->GetValue(v, i, ColumnIndex(HoistColumn::Yaw));
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

    table->GetValue(v, i, ColumnIndex(HoistColumn::ChainLength));
    double chainLen = 0.0;
    v.GetString().ToDouble(&chainLen);
    next.chainLength = static_cast<float>(chainLen);

    table->GetValue(v, i, ColumnIndex(HoistColumn::Capacity));
    float editedCapacityKg = old.capacityKg;
    if (const auto parsed = Units::ParseWeightToKilograms(
            std::string(v.GetString().ToUTF8()), weightUnit);
        parsed.has_value()) {
      editedCapacityKg = static_cast<float>(*parsed);
      next.capacityKg = editedCapacityKg;
    }

    table->GetValue(v, i, ColumnIndex(HoistColumn::Weight));
    float editedWeightKg = old.weightKg;
    if (const auto parsed = Units::ParseWeightToKilograms(
            std::string(v.GetString().ToUTF8()), weightUnit);
        parsed.has_value()) {
      editedWeightKg = static_cast<float>(*parsed);
      next.weightKg = editedWeightKg;
    }

    table->GetValue(v, i, ColumnIndex(HoistColumn::Load));
    if (const auto parsed = Units::ParseWeightToKilograms(
            std::string(v.GetString().ToUTF8()), weightUnit);
        parsed.has_value()) {
      next.loadKg = static_cast<float>(*parsed);
      auto automaticIt = pendingAutomaticLoadByUuid.find(old.uuid);
      if (automaticIt != pendingAutomaticLoadByUuid.end()) {
        next.loadSource =
            ShouldUseAutomaticHoistLoad(old.loadKg, next.loadKg,
                                        automaticIt->second)
                ? "Auto"
                : "Manual";
        if (next.loadSource == "Auto")
          next.loadKg = automaticIt->second;
      }
    }

    next.motorNameSource =
        ResolveHoistFieldDataSource(next.motorNameSource, next.hoistDataSource);
    next.capacitySource =
        ResolveHoistFieldDataSource(next.capacitySource, next.hoistDataSource);
    next.weightSource =
        ResolveHoistFieldDataSource(next.weightSource, next.hoistDataSource);
    next.hoistFunctionSource = ResolveHoistFieldDataSource(
        next.hoistFunctionSource, next.hoistDataSource);

    MarkTextFieldManualIfEdited(editedMotorName, oldEffective.motorName,
                                next.motorNameSource, old.motorName,
                                next.motorName);
    MarkNumericFieldManualIfEdited(editedCapacityKg, oldEffective.capacityKg,
                                   next.capacitySource, old.capacityKg,
                                   next.capacityKg);
    MarkNumericFieldManualIfEdited(editedWeightKg, oldEffective.weightKg,
                                   next.weightSource, old.weightKg,
                                   next.weightKg);
    MarkTextFieldManualIfEdited(editedHoistFunction, oldEffective.hoistFunction,
                                next.hoistFunctionSource, old.hoistFunction,
                                next.hoistFunction);

    const bool supportChanged =
        old.name != next.name || old.function != next.function ||
                                old.hoistFunction != next.hoistFunction ||
                                old.motorName != next.motorName ||
                                old.motorManufacturer != next.motorManufacturer ||
                                old.motorModel != next.motorModel ||
                                old.dummyProfileId != next.dummyProfileId ||
                                old.dummyPreset != next.dummyPreset ||
                                NormalizeHoistDataSource(old.hoistDataSource) !=
                                    NormalizeHoistDataSource(next.hoistDataSource) ||
        old.layer != next.layer || old.positionName != next.positionName ||
        transformChanged || old.chainLength != next.chainLength ||
        !Units::NearlyEqualWeightKilograms(old.capacityKg, next.capacityKg,
                                           0.001) ||
        !Units::NearlyEqualWeightKilograms(old.weightKg, next.weightKg,
                                           0.001) ||
                                !Units::NearlyEqualWeightKilograms(old.loadKg, next.loadKg, 0.001) ||
                                old.loadSource != next.loadSource ||
                                NormalizeHoistDataSource(old.motorNameSource) !=
                                    NormalizeHoistDataSource(next.motorNameSource) ||
                                NormalizeHoistDataSource(old.motorManufacturerSource) !=
                                    NormalizeHoistDataSource(next.motorManufacturerSource) ||
                                NormalizeHoistDataSource(old.motorModelSource) !=
                                    NormalizeHoistDataSource(next.motorModelSource) ||
                                NormalizeHoistDataSource(old.capacitySource) !=
                                    NormalizeHoistDataSource(next.capacitySource) ||
                                NormalizeHoistDataSource(old.weightSource) !=
                                    NormalizeHoistDataSource(next.weightSource) ||
                                NormalizeHoistDataSource(old.hoistFunctionSource) !=
                                    NormalizeHoistDataSource(next.hoistFunctionSource);
    const bool weightChanged =
        !Units::NearlyEqualWeightKilograms(old.weightKg, next.weightKg, 0.001);
    if (!supportChanged)
      continue;

    pushUndoIfNeeded();
    anyChanged = true;
    if (weightChanged) {
      changedWeightPositions.insert(NormalizePositionName(old.positionName));
      changedWeightPositions.insert(NormalizePositionName(next.positionName));
    }
    const Matrix requestedWorldTransform = next.transform;
    next.transform = old.transform;
    it->second = next;
    if (transformChanged) {
      scene_node_operations::ApplyExactWorldTransform(
          scene, MvrNodeType::Support, it->second.uuid,
          requestedWorldTransform);
    }
    if (!it->second.position.empty())
      scene.positions[it->second.position] = it->second.positionName;
  }

  if (!anyChanged) {
    pendingAutomaticLoadByUuid.clear();
    return;
  }

  pendingAutomaticLoadByUuid.clear();

  const bool loadsRecalculated = HoistLoadRecalculationPrompt::PromptAndApply(
      cfg, this, changedWeightPositions, false);
  if (loadsRecalculated)
    ReloadData();

  if (SummaryPanel::Instance())
    SummaryPanel::Instance()->ShowHoistSummary();
  if (RiggingPanel::Instance())
    RiggingPanel::Instance()->RefreshData();
  gui::sceneviewrefresh::RefreshLayout2DViewsAfterSceneChange(this);
}

HoistTablePanel *HoistTablePanel::Instance() { return s_instance; }

void HoistTablePanel::SetInstance(HoistTablePanel *panel) {
  s_instance = panel;
}

bool HoistTablePanel::IsActivePage() const {
  if (IsBeingDeleted())
    return false;
  wxWindow *parent = GetParent();
  if (!parent || parent->IsBeingDeleted())
    return false;
  auto *nb = dynamic_cast<wxNotebook *>(parent);
  if (!nb || nb->IsBeingDeleted())
    return false;
  const int selection = nb->GetSelection();
  if (selection == wxNOT_FOUND || selection < 0 ||
      selection >= static_cast<int>(nb->GetPageCount()))
    return false;
  return nb->GetPage(static_cast<size_t>(selection)) == this;
}

// Applies a primary hover highlight to one hoist row.
void HoistTablePanel::HighlightHoist(const std::string &uuid) {
  HighlightHoist(uuid, {});
}

// Applies primary and related group-hover highlights to hoist rows.
void HoistTablePanel::HighlightHoist(
    const std::string &uuid, const std::vector<std::string> &relatedUuids) {
  size_t count =
      std::min(rowUuids.size(), static_cast<size_t>(table->GetItemCount()));
  std::vector<bool> primaryRows(table->GetItemCount(), false);
  std::vector<bool> secondaryRows(table->GetItemCount(), false);
  for (size_t i = 0; i < count; ++i) {
    if (!uuid.empty() && rowUuids[i] == uuid)
      primaryRows[i] = true;
    else if (std::find(relatedUuids.begin(), relatedUuids.end(), rowUuids[i]) !=
             relatedUuids.end())
      secondaryRows[i] = true;
  }
  store->SetHighlightRows(primaryRows, secondaryRows, wxColour(170, 220, 0),
                          wxColour(110, 210, 150), wxColour(0, 0, 0));
  table->Refresh();
}

void HoistTablePanel::ClearSelection() {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContentChanged();
  table->UnselectAll();
  UpdateSelectionHighlight();
}

std::vector<std::string> HoistTablePanel::GetSelectedUuids() const {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<std::string> uuids;
  uuids.reserve(selections.size());
  for (const auto &it : selections) {
    const wxUIntPtr rowKey = store->GetItemData(it);
    auto keyIt = rowUuidByKey.find(rowKey);
    if (keyIt != rowUuidByKey.end())
      uuids.push_back(keyIt->second);
  }
  return uuids;
}

void HoistTablePanel::SelectByUuid(const std::vector<std::string> &uuids,
                                   bool notifySelectionChanged) {
  RebuildRowCachesFromRowKeys();
  std::unique_ptr<wxEventBlocker> selectionBlocker;
  if (!notifySelectionChanged) {
    selectionBlocker = std::make_unique<wxEventBlocker>(
        table, wxEVT_DATAVIEW_SELECTION_CHANGED);
  }
  table->UnselectAll();
  std::vector<bool> selectedRows(table->GetItemCount(), false);
  for (const auto &u : uuids) {
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

void HoistTablePanel::DeleteSelected(bool pushUndoState) {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContentChanged();
  RebuildRowCachesFromRowKeys();
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (selections.empty())
    return;

  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  if (pushUndoState)
    cfg.PushUndoState("delete support");
  cfg.SetSelectedSupports({});

  std::vector<int> rows;
  rows.reserve(selections.size());
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND)
      rows.push_back(r);
  }
  std::sort(rows.begin(), rows.end(), std::greater<int>());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

  auto &scene = cfg.GetScene();
  std::vector<scene_grouping::SceneTransformTarget> removalTargets;
  for (int r : rows) {
    if (r >= 0 && static_cast<size_t>(r) < rowUuids.size())
      removalTargets.push_back({MvrNodeType::Support, rowUuids[r]});
  }
  scene_node_operations::RemoveNodes(scene, removalTargets);
  for (int r : rows) {
    if ((size_t)r < rowUuids.size()) {
      wxDataViewItem rowItem = table->RowToItem(static_cast<unsigned int>(r));
      const wxUIntPtr rowKey = store->GetItemData(rowItem);
      rowUuids.erase(rowUuids.begin() + r);
      if ((size_t)r < rowLoadStates.size())
        rowLoadStates.erase(rowLoadStates.begin() + r);
      rowUuidByKey.erase(rowKey);
      loadStateByKey.erase(rowKey);
      table->DeleteItem(r);
    }
  }

  if (SummaryPanel::Instance())
    SummaryPanel::Instance()->ShowHoistSummary();

  if (RiggingPanel::Instance())
    RiggingPanel::Instance()->RefreshData();

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
  gui::sceneviewrefresh::RefreshLayout2DViewsAfterSceneChange(this);

  std::vector<std::string> order = rowUuids;
  ResyncRows(order, {});
}

void HoistTablePanel::ResyncRows(
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

void HoistTablePanel::RebuildRowCachesFromRowKeys() {
  if (!table || !store)
    return;
  const unsigned int count = table->GetItemCount();
  rowUuids.assign(count, std::string());
  rowLoadStates.assign(count, HoistLoadLimitUtils::LoadLimitState::Normal);
  for (unsigned int row = 0; row < count; ++row) {
    wxDataViewItem item = table->RowToItem(row);
    const wxUIntPtr rowKey = store->GetItemData(item);
    auto uuidIt = rowUuidByKey.find(rowKey);
    if (uuidIt != rowUuidByKey.end())
      rowUuids[row] = uuidIt->second;
    auto loadIt = loadStateByKey.find(rowKey);
    if (loadIt != loadStateByKey.end())
      rowLoadStates[row] = loadIt->second;
  }
}

std::string HoistTablePanel::UuidForItem(const wxDataViewItem &item) const {
  if (!store || !item.IsOk())
    return {};
  const wxUIntPtr rowKey = store->GetItemData(item);
  auto it = rowUuidByKey.find(rowKey);
  if (it == rowUuidByKey.end())
    return {};
  return it->second;
}

void HoistTablePanel::SetLoadStateForRow(
    unsigned int row, const HoistLoadLimitUtils::LoadLimitState &state) {
  if (!table || !store || row >= table->GetItemCount())
    return;
  if (row >= rowLoadStates.size())
    rowLoadStates.resize(table->GetItemCount(),
                         HoistLoadLimitUtils::LoadLimitState::Normal);
  rowLoadStates[row] = state;
  wxDataViewItem item = table->RowToItem(row);
  const wxUIntPtr rowKey = store->GetItemData(item);
  loadStateByKey[rowKey] = state;
}

// Reapplies UUID-based selection after user-driven column sorting changes row order.
void HoistTablePanel::OnColumnSorted(wxDataViewEvent &event) {
  if (deferredSelectionGuard)
    deferredSelectionGuard->NotifyContentChanged();
  RebuildRowCachesFromRowKeys();
  const std::vector<std::string> selectedUuids =
      guiConfigServices->LegacyConfigManager().GetSelectedSupports();
  std::vector<std::string> oldOrder = rowUuids;
  ResyncRows(oldOrder, selectedUuids);
  event.Skip();
}
