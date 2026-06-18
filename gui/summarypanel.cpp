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
#include "summarypanel.h"
#include "colorstore.h"
#include "configmanager.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "table_column_indices.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <map>
#include <unordered_set>
#include <wx/aui/framemanager.h>
#include <wx/colordlg.h>
#include <wx/dcmemory.h>

static SummaryPanel* s_instance = nullptr;

namespace {
using FixtureSummaryColumn = FixtureSummaryTableColumns::Column;
using ObjectSummaryColumn = ObjectSummaryTableColumns::Column;

// Converts a fixture summary column to its stable model index.
constexpr int FixtureColumnIndex(FixtureSummaryColumn column) {
    return TableColumnIndices::ToIndex(column);
}

// Converts an object summary column to its stable model index.
constexpr int ObjectColumnIndex(ObjectSummaryColumn column) {
    return TableColumnIndices::ToIndex(column);
}

bool IsRefreshTargetAlive(wxWindow* window) {
    return window && !window->IsBeingDeleted();
}
} // namespace

SummaryPanel::SummaryPanel(wxWindow* parent, ConfigManager* visibilityConfig,
                           ConfigManager* colorConfig)
    : wxPanel(parent, wxID_ANY),
      visibilityConfigManager(
          visibilityConfig
                                  ? visibilityConfig
                                  : &GetDefaultGuiConfigServices().LegacyConfigManager()),
      colorConfigManager(
          colorConfig
                             ? colorConfig
                             : (visibilityConfigManager
                                    ? visibilityConfigManager
                     : &GetDefaultGuiConfigServices().LegacyConfigManager())) {
    store = new ColorfulDataViewListStore();
  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxDV_ROW_LINES);
    table->AssociateModel(store);
    store->DecRef();

    EnsureColumnsForMode(SummaryMode::Generic);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
    BindTableEvents();
    ApplyInitialColumnWidths();
    CallAfter([this]() { ApplyInitialColumnWidths(); });
    Bind(wxEVT_SHOW, [this](wxShowEvent& evt) {
        if (evt.IsShown())
            ApplyInitialColumnWidths();
        evt.Skip();
    });
}

SummaryPanel *SummaryPanel::Instance() { return s_instance; }

void SummaryPanel::SetInstance(SummaryPanel *panel) { s_instance = panel; }

void SummaryPanel::SetVisibleRefreshTargets(Viewer2DPanel* viewer2D,
                                            Viewer3DPanel* viewer3D) {
    visibleRefreshViewer2D = viewer2D;
    visibleRefreshViewer3D = viewer3D;
}


void SummaryPanel::UpdatePaneCaption(const wxString& suffix) {
    wxAuiManager* manager = wxAuiManager::GetManager(this);
    if (!manager)
        return;

    wxAuiPaneInfo& pane = manager->GetPane(this);
    if (!pane.IsOk())
        return;

    const wxString caption = suffix.empty()
                                 ? wxString("Summary")
                                 : wxString::Format("Summary - %s", suffix);
    if (pane.caption == caption)
        return;

    pane.Caption(caption);
    manager->Update();
}

void SummaryPanel::ShowSummary(
    const std::vector<std::pair<std::string, int>> &items) {
  if (!table || !store)
    return;
    EnsureColumnsForMode(SummaryMode::Generic);
    store->DeleteAllItems();
    for (const auto& [name, count] : items) {
        wxVector<wxVariant> row;
        row.push_back(wxString::Format("%d", count));
        row.push_back(wxString::FromUTF8(name));
        store->AppendItem(row);
    }
}

void SummaryPanel::ShowFixtureSummary() {
    UpdatePaneCaption("Fixtures");
  if (!table || !store)
    return;
    std::map<std::string, FixtureSummaryRow> grouped;
    const auto& fixtures = (*visibilityConfigManager).GetScene().fixtures;
    for (const auto& [uuid, fix] : fixtures) {
        (void)uuid;
        auto& row = grouped[fix.typeName];
        row.typeName = fix.typeName;
        row.count += 1;
    if (row.visualColorHexHex.empty() && !fix.visualColorHex.empty())
      row.visualColorHexHex = fix.visualColorHex;
    }

    const auto hiddenFixtureTypes =
        (*visibilityConfigManager).GetHiddenFixtureTypes();
    std::vector<FixtureSummaryRow> rows;
    rows.reserve(grouped.size());
    for (auto& [typeName, row] : grouped) {
        row.visible = hiddenFixtureTypes.find(typeName) == hiddenFixtureTypes.end();
        rows.push_back(row);
    }
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return a.typeName < b.typeName;
    });
    ShowFixtureSummaryRows(rows);
}

void SummaryPanel::ShowTrussSummary() {
    UpdatePaneCaption("Trusses");
    std::map<std::string,int> counts;
    const auto& trusses = (*visibilityConfigManager).GetScene().trusses;
    for (const auto& [uuid, truss] : trusses)
        counts[truss.model]++;
    std::vector<std::pair<std::string,int>> items(counts.begin(), counts.end());
    ShowSummary(items);
}

void SummaryPanel::ShowHoistSummary() {
    UpdatePaneCaption("Hoists");
  if (!table)
    return;

    std::map<std::string, int> hoistCounts;
    const auto& supports = (*visibilityConfigManager).GetScene().supports;
    for (const auto& [uuid, support] : supports) {
        std::string type = support.function.empty() ? "Hoist" : support.function;
        hoistCounts[type]++;
    }
  std::vector<std::pair<std::string, int>> hoistItems(hoistCounts.begin(),
                                                      hoistCounts.end());

    ShowSummary(hoistItems);
}

void SummaryPanel::ShowSceneObjectSummary() {
    UpdatePaneCaption("Objects");
    std::map<std::string,int> counts;
    const auto& objs = (*visibilityConfigManager).GetScene().sceneObjects;
    for (const auto& [uuid, obj] : objs)
        counts[obj.name]++;
    std::vector<std::pair<std::string,int>> items(counts.begin(), counts.end());
    ShowSummary(items);
}

void SummaryPanel::ShowFixtureSummaryRows(
    const std::vector<FixtureSummaryRow>& rows) {
    EnsureColumnsForMode(SummaryMode::Fixture);
    store->DeleteAllItems();
    for (const auto& rowData : rows) {
        wxVector<wxVariant> row;
        row.push_back(wxVariant(rowData.visible));
        row.push_back(wxString::Format("%d", rowData.count));
        row.push_back(wxString::FromUTF8(rowData.typeName));

        wxBitmap bmp(16, 16);
        wxMemoryDC dc(bmp);
        wxColour color;
    if (!rowData.visualColorHexHex.empty() &&
        color.Set(wxString::FromUTF8(rowData.visualColorHexHex)))
            dc.SetBrush(wxBrush(color));
        else
            dc.SetBrush(wxBrush(wxColour(128, 128, 128)));
        dc.SetPen(*wxBLACK_PEN);
        dc.DrawRectangle(0, 0, 16, 16);
        dc.SelectObject(wxNullBitmap);
        wxDataViewIconText icon("", bmp);
        row.push_back(wxVariant(icon));
        store->AppendItem(row);
    }
    RefreshFixtureVisibilityStyles();
}

void SummaryPanel::EnsureColumnsForMode(SummaryMode requestedMode) {
    if (!table)
        return;
    if (mode == requestedMode && table->GetColumnCount() > 0)
        return;

    bool recreatedControl = false;
    while (table->GetColumnCount() > 0) {
    wxDataViewColumn *firstColumn =
        table->GetColumn(ObjectColumnIndex(ObjectSummaryColumn::CountValue));
        if (!firstColumn)
            break;
        if (!table->DeleteColumn(firstColumn)) {
            RecreateTableControl();
            recreatedControl = true;
            break;
        }
    }
    if (recreatedControl && table->GetColumnCount() > 0)
        return;

    if (requestedMode == SummaryMode::Fixture) {
        table->AppendToggleColumn("Visible", wxDATAVIEW_CELL_ACTIVATABLE, 70,
                                  wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
        table->AppendTextColumn("Count", wxDATAVIEW_CELL_INERT, 60, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        table->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 150, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        auto* colorRenderer = new wxDataViewIconTextRenderer();
        table->AppendColumn(new wxDataViewColumn(
        "Color", colorRenderer, FixtureColumnIndex(FixtureSummaryColumn::Color),
        80, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE));
    } else {
        table->AppendTextColumn("Count", wxDATAVIEW_CELL_INERT, 60, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        table->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 150, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
    }
    mode = requestedMode;
    ApplyInitialColumnWidths();
}

void SummaryPanel::BindTableEvents() {
    if (!table)
        return;

    table->Bind(wxEVT_SIZE, [this](wxSizeEvent& evt) {
        ApplyInitialColumnWidths();
        evt.Skip();
    });
    table->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED,
                &SummaryPanel::OnItemValueChanged, this);
  table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &SummaryPanel::OnItemActivated,
              this);
    table->Bind(wxEVT_MOTION, &SummaryPanel::OnMouseMove, this);
    table->Bind(wxEVT_LEAVE_WINDOW, &SummaryPanel::OnMouseLeave, this);
}

void SummaryPanel::ApplyInitialColumnWidths() {
    if (!table)
        return;
    if (table->GetColumnCount() == 0)
        return;

    const int tableWidth = std::max(0, table->GetClientSize().GetWidth());
    if (mode == SummaryMode::Fixture) {
    auto *visibleColumn =
        table->GetColumn(FixtureColumnIndex(FixtureSummaryColumn::Visible));
    auto *countColumn =
        table->GetColumn(FixtureColumnIndex(FixtureSummaryColumn::CountValue));
    auto *typeColumn =
        table->GetColumn(FixtureColumnIndex(FixtureSummaryColumn::Type));
    auto *colorColumn =
        table->GetColumn(FixtureColumnIndex(FixtureSummaryColumn::Color));
        if (!visibleColumn || !countColumn || !typeColumn || !colorColumn)
            return;

        wxClientDC dc(table);
        dc.SetFont(table->GetFont());
        int visibleLabelWidth = 0;
        int countLabelWidth = 0;
        int colorLabelWidth = 0;
        dc.GetTextExtent("Visible", &visibleLabelWidth, nullptr);
        dc.GetTextExtent("Count", &countLabelWidth, nullptr);
        dc.GetTextExtent("Color", &colorLabelWidth, nullptr);

        const int visibleWidth = visibleLabelWidth + 30;
        const int countWidth = countLabelWidth + 20;
        const int colorWidth = std::max(60, colorLabelWidth + 24);
    const int typeWidth =
        std::max(120, tableWidth - visibleWidth - countWidth - colorWidth - 8);

        visibleColumn->SetMinWidth(visibleWidth);
        visibleColumn->SetWidth(visibleWidth);
        countColumn->SetMinWidth(countWidth);
        countColumn->SetWidth(countWidth);
        typeColumn->SetMinWidth(120);
        typeColumn->SetWidth(typeWidth);
        colorColumn->SetMinWidth(colorWidth);
        colorColumn->SetWidth(colorWidth);
        return;
    }

  auto *countColumn =
      table->GetColumn(ObjectColumnIndex(ObjectSummaryColumn::CountValue));
  auto *typeColumn =
      table->GetColumn(ObjectColumnIndex(ObjectSummaryColumn::Type));
    if (!countColumn || !typeColumn)
        return;

    wxClientDC dc(table);
    dc.SetFont(table->GetFont());
    int countLabelWidth = 0;
    dc.GetTextExtent("Count", &countLabelWidth, nullptr);
    const int countWidth = countLabelWidth + 20;
    const int typeWidth = std::max(120, tableWidth - countWidth - 8);
    countColumn->SetMinWidth(countWidth);
    countColumn->SetWidth(countWidth);
    typeColumn->SetMinWidth(120);
    typeColumn->SetWidth(typeWidth);
}

void SummaryPanel::RecreateTableControl() {
    if (!table || !store)
        return;

    // Keep the model alive while the old control is destroyed and a new one
    // takes ownership. This avoids a dangling model pointer on macOS.
    store->IncRef();

    wxSizer* sizer = GetSizer();
    if (sizer)
        sizer->Detach(table);
    table->Destroy();

  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxDV_ROW_LINES);
    table->AssociateModel(store);
    store->DecRef();
    BindTableEvents();

    if (sizer) {
        sizer->Add(table, 1, wxEXPAND | wxALL, 5);
        Layout();
    }
}

void SummaryPanel::RefreshFixtureVisibilityStyles() {
    if (!table || !store || mode != SummaryMode::Fixture)
        return;

    const auto hiddenFixtureTypes =
        (*visibilityConfigManager).GetHiddenFixtureTypes();
    for (unsigned int row = 0; row < table->GetItemCount(); ++row) {
        store->ClearRowTextColour(row);
    wxString typeName = table->GetTextValue(
        static_cast<int>(row), FixtureColumnIndex(FixtureSummaryColumn::Type));
    if (hiddenFixtureTypes.find(typeName.ToStdString()) !=
        hiddenFixtureTypes.end())
            store->SetRowTextColour(row, *wxRED);
    }
    table->Refresh();
}

void SummaryPanel::RefreshVisibleViewers() const {
  Viewer2DPanel *viewer2D = visibleRefreshViewer2D ? visibleRefreshViewer2D
                                  : Viewer2DPanel::Instance();
  Viewer3DPanel *viewer3D = visibleRefreshViewer3D ? visibleRefreshViewer3D
                                  : Viewer3DPanel::Instance();
    if (IsRefreshTargetAlive(viewer2D)) {
        viewer2D->UpdateScene(true);
        viewer2D->Refresh();
    }
    if (IsRefreshTargetAlive(viewer3D)) {
        viewer3D->UpdateScene();
        viewer3D->Refresh();
    }
}

void SummaryPanel::OnItemValueChanged(wxDataViewEvent& event) {
    if (!table || mode != SummaryMode::Fixture ||
      event.GetColumn() != FixtureColumnIndex(FixtureSummaryColumn::Visible))
        return;
    const int row = table->ItemToRow(event.GetItem());
    if (row == wxNOT_FOUND)
        return;

    wxVariant value = event.GetValue();
    if (!value.IsType("bool"))
    table->GetValue(value, row,
                    FixtureColumnIndex(FixtureSummaryColumn::Visible));
    const bool visible = value.GetBool();
  const std::string typeName =
      table->GetTextValue(row, FixtureColumnIndex(FixtureSummaryColumn::Type))
          .ToStdString();

  auto hiddenFixtureTypes = (*visibilityConfigManager).GetHiddenFixtureTypes();
    if (visible)
        hiddenFixtureTypes.erase(typeName);
    else
        hiddenFixtureTypes.insert(typeName);
    (*visibilityConfigManager).SetHiddenFixtureTypes(hiddenFixtureTypes);
    CallAfter([this]() {
        if (table)
            table->UnselectAll();
        RefreshFixtureVisibilityStyles();
        RefreshVisibleViewers();
    });
}

void SummaryPanel::OnItemActivated(wxDataViewEvent& event) {
    if (!table || mode != SummaryMode::Fixture)
        return;
    const int row = table->ItemToRow(event.GetItem());
    if (row == wxNOT_FOUND ||
        event.GetColumn() != FixtureColumnIndex(FixtureSummaryColumn::Color))
        return;

    const std::string typeName =
      table->GetTextValue(row, FixtureColumnIndex(FixtureSummaryColumn::Type))
            .ToStdString();
    auto& colorCfg = (*colorConfigManager);
    auto& fixtures = colorCfg.GetScene().fixtures;

    wxColourData data;
    for (const auto& [uuid, fixture] : fixtures) {
        (void)uuid;
    if (fixture.typeName == typeName && !fixture.visualColorHex.empty()) {
      data.SetColour(wxColour(wxString::FromUTF8(fixture.visualColorHex)));
            break;
        }
    }

    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() != wxID_OK)
        return;

    const wxColour color = dlg.GetColourData().GetColour();
  const std::string hex = wxString::Format("#%02X%02X%02X", color.Red(),
                                           color.Green(), color.Blue())
                              .ToStdString();
    colorCfg.PushUndoState("change fixture type color in project from summary");
    for (auto& [uuid, fixture] : fixtures) {
        (void)uuid;
        if (fixture.typeName == typeName)
      fixture.visualColorHex = hex;
    }

    if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->ReloadData();

    wxBitmap bmp(16, 16);
    wxMemoryDC dc(bmp);
    dc.SetBrush(wxBrush(color));
    dc.SetPen(*wxBLACK_PEN);
    dc.DrawRectangle(0, 0, 16, 16);
    dc.SelectObject(wxNullBitmap);
  table->SetValue(wxVariant(wxDataViewIconText("", bmp)), row,
                  FixtureColumnIndex(FixtureSummaryColumn::Color));
    RefreshVisibleViewers();
}

void SummaryPanel::OnMouseMove(wxMouseEvent& event) {
    if (!table || mode != SummaryMode::Fixture) {
        event.Skip();
        return;
    }

    wxDataViewItem item;
    wxDataViewColumn* column = nullptr;
    table->HitTest(event.GetPosition(), item, column);

    wxString tooltip;
    if (item.IsOk()) {
        const int row = table->ItemToRow(item);
        if (row != wxNOT_FOUND) {
            wxVariant value;
      table->GetValue(value, row,
                      FixtureColumnIndex(FixtureSummaryColumn::Visible));
            if (!value.GetBool())
                tooltip = "Fixture type hidden: not rendered in 2D/3D viewers.";
        }
    }

    if (tooltip != activeHoverTooltip) {
        table->SetToolTip(tooltip);
        activeHoverTooltip = tooltip;
    }
    event.Skip();
}

void SummaryPanel::OnMouseLeave(wxMouseEvent& event) {
    if (activeHoverTooltip.empty()) {
        event.Skip();
        return;
    }
    activeHoverTooltip.clear();
    if (table)
        table->SetToolTip(wxString());
    event.Skip();
}
