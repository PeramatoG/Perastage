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

#include "columnutils.h"
#include "colorfulrenderers.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "layerpanel.h"
#include "dummyprofilelibrary.h"
#include "matrixutils.h"
#include "riggingpanel.h"
#include "stringutils.h"
#include "summarypanel.h"
#include "dataview_edit_commit.h"
#include "support.h"
#include "ui_unit_utils.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <wx/choicdlg.h>
#include <wx/notebook.h>
#include <wx/wupdlock.h> // freeze/thaw UI during batch edits
#include <wx/version.h>

static HoistTablePanel *s_instance = nullptr;

namespace {

const wxString &DegreeSymbol() {
  static const wxString kDegreeSymbol = wxString::FromUTF8("\xC2\xB0");
  return kDegreeSymbol;
}

UiUnitUtils::DistanceUnitSystem ResolveDistanceUnitSystem() {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return UiUnitUtils::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
}

UiUnitUtils::WeightUnitSystem ResolveWeightUnitSystem() {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return UiUnitUtils::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
}

struct RangeParts {
  wxArrayString parts;
  bool usedSeparator = false;
  bool trailingSeparator = false;
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

void SetAllHoistFieldSources(Support &support, const std::string &source) {
  const std::string normalized = NormalizeHoistDataSource(source);
  support.motorNameSource = normalized;
  support.motorManufacturerSource = normalized;
  support.motorModelSource = normalized;
  support.capacitySource = normalized;
  support.weightSource = normalized;
  support.hoistFunctionSource = normalized;
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


std::optional<HoistFixtureDefaults> FindFixtureDefaults(const MvrScene &scene,
                                                       const Support &support) {
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

HoistTablePanel::HoistTablePanel(wxWindow *parent, IGuiConfigServices *services)
    : wxPanel(parent, wxID_ANY), guiConfigServices(services ? services : &GetDefaultGuiConfigServices()) {
  store = new ColorfulDataViewListStore();
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxDV_MULTIPLE | wxDV_ROW_LINES);
  table->AssociateModel(store);
  store->DecRef();

  table->SetAlternateRowColour(wxColour(40, 40, 40));
  const wxColour selectionBackground(0, 255, 255);
  const wxColour selectionForeground(0, 0, 0);
  store->SetSelectionColours(selectionBackground, selectionForeground);
  table->Bind(wxEVT_LEFT_DOWN, &HoistTablePanel::OnLeftDown, this);
  table->Bind(wxEVT_LEFT_UP, &HoistTablePanel::OnLeftUp, this);
  table->Bind(wxEVT_MOTION, &HoistTablePanel::OnMouseMove, this);
  table->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
              &HoistTablePanel::OnSelectionChanged, this);

  table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &HoistTablePanel::OnContextMenu,
              this);
  table->Bind(wxEVT_DATAVIEW_COLUMN_SORTED, &HoistTablePanel::OnColumnSorted,
              this);

  Bind(wxEVT_MOUSE_CAPTURE_LOST, &HoistTablePanel::OnCaptureLost, this);

  InitializeTable();
  ReloadData();

  sizer->Add(table, 1, wxEXPAND | wxALL, 5);
  SetSizer(sizer);
}

HoistTablePanel::~HoistTablePanel() { store = nullptr; }

void HoistTablePanel::InitializeTable() {
  const auto distanceUnit = ResolveDistanceUnitSystem();
  const auto weightUnit = ResolveWeightUnitSystem();
  const wxString distanceSuffix = wxString::FromUTF8(UiUnitUtils::DistanceUnitSuffix(distanceUnit));
  const wxString weightSuffix = wxString::FromUTF8(UiUnitUtils::WeightUnitSuffix(weightUnit));
  columnLabels = {"Hoist ID",      "Name",          "Type",      "Function",
                  "Motor",         "Dummy Preset",  "Data Source", "Layer",
                  "Hang Pos",      "Pos X (" + distanceSuffix + ")",         "Pos Y (" + distanceSuffix + ")",     "Pos Z (" + distanceSuffix + ")",
                  "Roll (X)",      "Pitch (Y)",     "Yaw (Z)",
                  "Chain Length (m)", "Capacity (" + weightSuffix + ")", "Weight (" + weightSuffix + ")",
                  "Load (" + weightSuffix + ")"};
  std::vector<int> widths = {70, 150, 120, 120, 130, 150, 110, 100, 120,
                             80, 80, 80, 80, 80, 80, 110, 110, 100, 100};
  for (size_t i = 0; i < columnLabels.size(); ++i)
    table->AppendColumn(new wxDataViewColumn(
        columnLabels[i], new ColorfulTextRenderer(wxDATAVIEW_CELL_INERT,
                                                  wxALIGN_LEFT),
        i, widths[i], wxALIGN_LEFT,
        wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
  ColumnUtils::EnforceMinColumnWidth(table);
}

void HoistTablePanel::ReloadData() {
  table->DeleteAllItems();
  rowUuids.clear();
  const MvrScene &scene = guiConfigServices->LegacyConfigManager().GetScene();
  auto &supports = guiConfigServices->LegacyConfigManager().GetScene().supports;

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
    support.motorNameSource =
        ResolveHoistFieldDataSource(support.motorNameSource, support.hoistDataSource);
    support.motorManufacturerSource = ResolveHoistFieldDataSource(
        support.motorManufacturerSource, support.hoistDataSource);
    support.motorModelSource =
        ResolveHoistFieldDataSource(support.motorModelSource, support.hoistDataSource);
    support.capacitySource =
        ResolveHoistFieldDataSource(support.capacitySource, support.hoistDataSource);
    support.weightSource =
        ResolveHoistFieldDataSource(support.weightSource, support.hoistDataSource);
    support.hoistFunctionSource = ResolveHoistFieldDataSource(
        support.hoistFunctionSource, support.hoistDataSource);
    const auto effective =
        ResolveEffectiveSupportData(support, FindPresetDefaults(support),
                                    FindFixtureDefaults(scene, support));
    support.hoistFunction = NormalizeHoistFunction(support.hoistFunction);
    wxString hoistFunction = wxString::FromUTF8(effective.hoistFunction);
    wxString motorName = wxString::FromUTF8(effective.motorName);
    wxString dummyPreset = wxString::FromUTF8(ResolveDummyProfileDisplayName(support));
    wxString dataSource = wxString::FromUTF8(support.hoistDataSource);
    wxString layer = support.layer == DEFAULT_LAYER_NAME
                         ? wxString()
                         : wxString::FromUTF8(support.layer);
    wxString posName = wxString::FromUTF8(support.positionName);

    const auto distanceUnit = ResolveDistanceUnitSystem();
    const auto weightUnit = ResolveWeightUnitSystem();
    auto posArr = support.transform.o;
    wxString posX = wxString::FromUTF8(UiUnitUtils::FormatDistanceFromMillimeters(
        posArr[0], distanceUnit, UiUnitUtils::ValueFormatContext::Table));
    wxString posY = wxString::FromUTF8(UiUnitUtils::FormatDistanceFromMillimeters(
        posArr[1], distanceUnit, UiUnitUtils::ValueFormatContext::Table));
    wxString posZ = wxString::FromUTF8(UiUnitUtils::FormatDistanceFromMillimeters(
        posArr[2], distanceUnit, UiUnitUtils::ValueFormatContext::Table));

    auto euler = MatrixUtils::MatrixToEuler(support.transform);
    wxString roll = wxString::Format("%.1f", euler[2]) + DegreeSymbol();
    wxString pitch = wxString::Format("%.1f", euler[1]) + DegreeSymbol();
    wxString yaw = wxString::Format("%.1f", euler[0]) + DegreeSymbol();

    wxString chainLen = wxString::Format("%.2f", support.chainLength);
    wxString capacity = wxString::FromUTF8(UiUnitUtils::FormatWeightFromKilograms(
        effective.capacityKg, weightUnit, UiUnitUtils::ValueFormatContext::Table));
    wxString weight = wxString::FromUTF8(UiUnitUtils::FormatWeightFromKilograms(
        effective.weightKg, weightUnit, UiUnitUtils::ValueFormatContext::Table));
    wxString load = wxString::FromUTF8(UiUnitUtils::FormatWeightFromKilograms(
        support.loadKg, weightUnit, UiUnitUtils::ValueFormatContext::Table));

    row.push_back(name);
    row.push_back(type);
    row.push_back(hoistFunction);
    row.push_back(motorName);
    row.push_back(dummyPreset);
    row.push_back(dataSource);
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

    store->AppendItem(row, rowUuids.size());
    rowUuids.push_back(uuid);
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
  wxDataViewItem item = event.GetItem();
  int col = event.GetColumn();
  if (!item.IsOk() || col < 0)
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

  if (col == 3) {
    wxArrayString choices;
    for (const auto &option : GetHoistFunctionOptions())
      choices.push_back(wxString::FromUTF8(option));
    choices.push_back("Other...");

    wxSingleChoiceDialog sdlg(this, "Select function", "Function", choices);
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
      wxTextEntryDialog otherDlg(this, "Enter function", "Function",
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

  if (col == 5) {
    ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
    const auto &supports = cfg.GetScene().supports;

    wxArrayString choices = BuildDummyPresetChoices();
    wxSingleChoiceDialog sdlg(this, "Select dummy preset", "Dummy Preset", choices);
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
      wxMessageBox("Dummy preset can only be assigned when there is no linked motor fixture.",
                   "Dummy Preset", wxOK | wxICON_INFORMATION, this);
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

  if (col == 6) {
    wxArrayString choices;
    choices.push_back("Inherited");
    choices.push_back("Manual");
    wxSingleChoiceDialog sdlg(this, "Select data source", "Data Source", choices);
    if (sdlg.ShowModal() != wxID_OK)
      return;
    wxString sel = sdlg.GetStringSelection();
    for (const auto &itSel : selections) {
      int r = table->ItemToRow(itSel);
      if (r != wxNOT_FOUND)
        table->SetValue(wxVariant(sel), r, col);
    }
    ResyncRows(oldOrder, selectedUuids);
    UpdateSceneData();
    ReloadData();
    if (Viewer3DPanel::Instance()) {
      Viewer3DPanel::Instance()->UpdateScene();
      Viewer3DPanel::Instance()->Refresh();
    } else if (Viewer2DPanel::Instance()) {
      Viewer2DPanel::Instance()->UpdateScene();
    }
    return;
  }

  if (col == 7) {
    auto layers = guiConfigServices->LegacyConfigManager().GetLayerNames();
    wxArrayString choices;
    for (const auto &n : layers)
      choices.push_back(wxString::FromUTF8(n));
    wxSingleChoiceDialog sdlg(this, "Select layer", "Layer", choices);
    if (sdlg.ShowModal() != wxID_OK)
      return;
    wxString sel = sdlg.GetStringSelection();
    wxString val = sel == wxString::FromUTF8(DEFAULT_LAYER_NAME) ? wxString()
                                                                 : sel;
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

  wxTextEntryDialog dlg(this, "Edit value:", columnLabels[col],
                        current.GetString());
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString value = dlg.GetValue().Trim(true).Trim(false);

  bool numericCol = (col >= 9);
  bool relative = false;
  double delta = 0.0;
  if (numericCol && col <= 18 &&
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
        if (col >= 12 && col <= 14) {
          if (!DegreeSymbol().empty())
            cur.Replace(DegreeSymbol(), "");
        }
        double curVal = 0.0;
        cur.ToDouble(&curVal);
        double newVal = curVal + delta;
        wxString out;
        if (col >= 12 && col <= 14)
          out = wxString::Format("%.1f", newVal) + DegreeSymbol();
        else
          out = wxString::Format((col >= 15) ? "%.2f" : "%.3f", newVal);
        table->SetValue(wxVariant(out), r, col);
      }
    } else {
      RangeParts range = SplitRangeParts(value);
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
        if (col >= 12 && col <= 14)
          out = wxString::Format("%.1f", val) + DegreeSymbol();
        else
          out = wxString::Format((col >= 15) ? "%.2f" : "%.3f", val);

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
    table->UnselectAll();
    table->SelectRow(startRow);
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
  if (!dragSelecting || !evt.Dragging()) {
    evt.Skip();
    return;
  }
  wxDataViewItem item;
  wxDataViewColumn *col;
  table->HitTest(evt.GetPosition(), item, col);
  int row = table->ItemToRow(item);
  if (row != wxNOT_FOUND) {
    int minRow = std::min(startRow, row);
    int maxRow = std::max(startRow, row);
    table->UnselectAll();
    for (int r = minRow; r <= maxRow; ++r)
      table->SelectRow(r);
  }
  evt.Skip();
}

void HoistTablePanel::OnSelectionChanged(wxDataViewEvent &evt) {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<std::string> uuids;
  uuids.reserve(selections.size());
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
      uuids.push_back(rowUuids[r]);
  }
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  if (uuids != cfg.GetSelectedSupports()) {
    cfg.PushUndoState("support selection");
    cfg.SetSelectedSupports(uuids);
  }
  if (Viewer3DPanel::Instance())
    Viewer3DPanel::Instance()->SetSelectedFixtures(uuids);
  if (Viewer2DPanel::Instance())
    Viewer2DPanel::Instance()->SetSelectedUuids(uuids);
  UpdateSelectionHighlight();
  evt.Skip();
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
    table->SetValue(wxVariant(wxString::FromUTF8(update.posX)), row, 9);
    table->SetValue(wxVariant(wxString::FromUTF8(update.posY)), row, 10);
    table->SetValue(wxVariant(wxString::FromUTF8(update.posZ)), row, 11);
  }
}

void HoistTablePanel::UpdateSceneData(bool logChanges) {
  // Ensure in-place cell editors commit pending values before reading table rows.
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);
  (void)logChanges;
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  auto &scene = cfg.GetScene();
  size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());
  bool anyChanged = false;
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
    table->GetValue(v, i, 1);
    next.name = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 2);
    next.function = std::string(v.GetString().ToUTF8());

    const auto oldEffective =
        ResolveEffectiveSupportData(old, FindPresetDefaults(old),
                                    FindFixtureDefaults(scene, old));

    table->GetValue(v, i, 3);
    const std::string editedHoistFunction =
        NormalizeHoistFunction(std::string(v.GetString().ToUTF8()));
    next.hoistFunction = editedHoistFunction;

    table->GetValue(v, i, 4);
    const std::string editedMotorName = std::string(v.GetString().ToUTF8());
    next.motorName = editedMotorName;

    table->GetValue(v, i, 5);
    next.dummyPreset = std::string(v.GetString().ToUTF8());
    if (next.dummyPreset.empty()) {
      next.dummyProfileId.clear();
    } else {
      const auto profile = DummyProfileLibrary::FindByDisplayName(next.dummyPreset);
      next.dummyProfileId = profile.has_value() ? profile->id : "";
    }

    table->GetValue(v, i, 6);
    next.hoistDataSource =
        NormalizeHoistDataSource(std::string(v.GetString().ToUTF8()));
    if (IsManualHoistDataSource(next.hoistDataSource))
      SetAllHoistFieldSources(next, "Manual");
    else if (NormalizeHoistDataSource(old.hoistDataSource) != "Inherited")
      SetAllHoistFieldSources(next, "Inherited");

    table->GetValue(v, i, 7);
    std::string layerStr = std::string(v.GetString().ToUTF8());
    if (layerStr.empty())
      next.layer.clear();
    else
      next.layer = layerStr;

    table->GetValue(v, i, 8);
    next.positionName = std::string(v.GetString().ToUTF8());

    const auto distanceUnit = ResolveDistanceUnitSystem();
    const auto weightUnit = ResolveWeightUnitSystem();
    double xMm = old.transform.o[0], yMm = old.transform.o[1], zMm = old.transform.o[2];
    table->GetValue(v, i, 9);
    if (const auto parsed = UiUnitUtils::ParseDistanceToMillimeters(std::string(v.GetString().ToUTF8()), distanceUnit); parsed.has_value())
      xMm = *parsed;
    table->GetValue(v, i, 10);
    if (const auto parsed = UiUnitUtils::ParseDistanceToMillimeters(std::string(v.GetString().ToUTF8()), distanceUnit); parsed.has_value())
      yMm = *parsed;
    table->GetValue(v, i, 11);
    if (const auto parsed = UiUnitUtils::ParseDistanceToMillimeters(std::string(v.GetString().ToUTF8()), distanceUnit); parsed.has_value())
      zMm = *parsed;

    double roll = 0, pitch = 0, yaw = 0;
    table->GetValue(v, i, 12);
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
      s.ToDouble(&roll);
    }
    table->GetValue(v, i, 13);
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
      s.ToDouble(&pitch);
    }
    table->GetValue(v, i, 14);
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
            s.Replace(DegreeSymbol(), "");
      s.ToDouble(&yaw);
    }

    const auto currentEuler = MatrixUtils::MatrixToEuler(old.transform);
    const bool transformChanged =
        !UiUnitUtils::NearlyEqualDistanceMillimeters(old.transform.o[0], xMm, 0.5) ||
        !UiUnitUtils::NearlyEqualDistanceMillimeters(old.transform.o[1], yMm, 0.5) ||
        !UiUnitUtils::NearlyEqualDistanceMillimeters(old.transform.o[2], zMm, 0.5) ||
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

    table->GetValue(v, i, 15);
    double chainLen = 0.0;
    v.GetString().ToDouble(&chainLen);
    next.chainLength = static_cast<float>(chainLen);

    table->GetValue(v, i, 16);
    float editedCapacityKg = old.capacityKg;
    if (const auto parsed = UiUnitUtils::ParseWeightToKilograms(std::string(v.GetString().ToUTF8()), weightUnit); parsed.has_value()) {
      editedCapacityKg = static_cast<float>(*parsed);
      next.capacityKg = editedCapacityKg;
    }

    table->GetValue(v, i, 17);
    float editedWeightKg = old.weightKg;
    if (const auto parsed = UiUnitUtils::ParseWeightToKilograms(std::string(v.GetString().ToUTF8()), weightUnit); parsed.has_value()) {
      editedWeightKg = static_cast<float>(*parsed);
      next.weightKg = editedWeightKg;
    }

    table->GetValue(v, i, 18);
    if (const auto parsed = UiUnitUtils::ParseWeightToKilograms(std::string(v.GetString().ToUTF8()), weightUnit); parsed.has_value())
      next.loadKg = static_cast<float>(*parsed);

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

    const bool supportChanged = old.name != next.name || old.function != next.function ||
                                old.hoistFunction != next.hoistFunction ||
                                old.motorName != next.motorName ||
                                old.motorManufacturer != next.motorManufacturer ||
                                old.motorModel != next.motorModel ||
                                old.dummyProfileId != next.dummyProfileId ||
                                old.dummyPreset != next.dummyPreset ||
                                NormalizeHoistDataSource(old.hoistDataSource) !=
                                    NormalizeHoistDataSource(next.hoistDataSource) ||
                                old.layer != next.layer ||
                                old.positionName != next.positionName || transformChanged ||
                                old.chainLength != next.chainLength ||
                                !UiUnitUtils::NearlyEqualWeightKilograms(old.capacityKg, next.capacityKg, 0.001) ||
                                !UiUnitUtils::NearlyEqualWeightKilograms(old.weightKg, next.weightKg, 0.001) ||
                                !UiUnitUtils::NearlyEqualWeightKilograms(old.loadKg, next.loadKg, 0.001) ||
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
    if (!supportChanged)
      continue;

    pushUndoIfNeeded();
    anyChanged = true;
    it->second = next;
    if (!it->second.position.empty())
      scene.positions[it->second.position] = it->second.positionName;
  }

  if (!anyChanged)
    return;

  if (SummaryPanel::Instance())
    SummaryPanel::Instance()->ShowHoistSummary();
  if (RiggingPanel::Instance())
    RiggingPanel::Instance()->RefreshData();
}


HoistTablePanel *HoistTablePanel::Instance() { return s_instance; }

void HoistTablePanel::SetInstance(HoistTablePanel *panel) { s_instance = panel; }

bool HoistTablePanel::IsActivePage() const {
  auto *nb = dynamic_cast<wxNotebook *>(GetParent());
  return nb && nb->GetPage(nb->GetSelection()) == this;
}

void HoistTablePanel::HighlightHoist(const std::string &uuid) {
  size_t count =
      std::min(rowUuids.size(), static_cast<size_t>(table->GetItemCount()));
  for (size_t i = 0; i < count; ++i) {
    if (!uuid.empty() && rowUuids[i] == uuid)
      store->SetRowBackgroundColour(i, wxColour(0, 200, 0));
    else
      store->ClearRowBackground(i);
  }
  table->Refresh();
}

void HoistTablePanel::ClearSelection() {
  table->UnselectAll();
  UpdateSelectionHighlight();
}

std::vector<std::string> HoistTablePanel::GetSelectedUuids() const {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<std::string> uuids;
  uuids.reserve(selections.size());
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
      uuids.push_back(rowUuids[r]);
  }
  return uuids;
}

void HoistTablePanel::SelectByUuid(const std::vector<std::string> &uuids) {
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

void HoistTablePanel::DeleteSelected() {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (selections.empty())
    return;

  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
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
  for (int r : rows) {
    if ((size_t)r < rowUuids.size()) {
      scene.supports.erase(rowUuids[r]);
      rowUuids.erase(rowUuids.begin() + r);
      table->DeleteItem(r);
    }
  }

  if (SummaryPanel::Instance())
    SummaryPanel::Instance()->ShowHoistSummary();

  if (RiggingPanel::Instance())
    RiggingPanel::Instance()->RefreshData();

  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->SetSelectedFixtures({});
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  } else if (Viewer2DPanel::Instance()) {
    Viewer2DPanel::Instance()->SetSelectedUuids({});
    Viewer2DPanel::Instance()->UpdateScene();
  }

  std::vector<std::string> order = rowUuids;
  ResyncRows(order, {});
}

void HoistTablePanel::ResyncRows(const std::vector<std::string> &oldOrder,
                                 const std::vector<std::string> &selectedUuids) {
  unsigned int count = table->GetItemCount();
  std::vector<std::string> newOrder(count);
  for (unsigned int i = 0; i < count; ++i) {
    wxDataViewItem it = table->RowToItem(i);
    unsigned long idx = store->GetItemData(it);
    if (idx < oldOrder.size())
      newOrder[i] = oldOrder[idx];
    store->SetItemData(it, i);
  }
  rowUuids.swap(newOrder);

  table->UnselectAll();
  for (const auto &uuid : selectedUuids) {
    auto pos = std::find(rowUuids.begin(), rowUuids.end(), uuid);
    if (pos != rowUuids.end())
      table->SelectRow(static_cast<int>(pos - rowUuids.begin()));
  }
  UpdateSelectionHighlight();
}

void HoistTablePanel::OnColumnSorted(wxDataViewEvent &event) {
  wxDataViewItemArray selections;
  table->GetSelections(selections);
  std::vector<std::string> selectedUuids;
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r != wxNOT_FOUND && (size_t)r < rowUuids.size())
      selectedUuids.push_back(rowUuids[r]);
  }
  std::vector<std::string> oldOrder = rowUuids;
  ResyncRows(oldOrder, selectedUuids);
  event.Skip();
}
