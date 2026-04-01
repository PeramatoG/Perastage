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
#include "guiconfigservices.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <wx/colordlg.h>
#include <wx/dcmemory.h>
#include <algorithm>
#include <map>
#include <unordered_set>

static SummaryPanel* s_instance = nullptr;

SummaryPanel::SummaryPanel(wxWindow* parent, ConfigManager* visibilityConfig,
                           ConfigManager* colorConfig)
    : wxPanel(parent, wxID_ANY),
      visibilityConfigManager(visibilityConfig
                                  ? visibilityConfig
                                  : &GetDefaultGuiConfigServices().LegacyConfigManager()),
      colorConfigManager(colorConfig
                             ? colorConfig
                             : (visibilityConfigManager
                                    ? visibilityConfigManager
                                    : &GetDefaultGuiConfigServices().LegacyConfigManager()))
{
    store = new ColorfulDataViewListStore();
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES);
    table->AssociateModel(store);
    store->DecRef();

    EnsureColumnsForMode(SummaryMode::Generic);

    auto applyInitialColumnWidths = [this]() {
        if (!table)
            return;
        if (table->GetColumnCount() == 0)
            return;

        const int tableWidth = std::max(0, table->GetClientSize().GetWidth());
        if (mode == SummaryMode::Fixture) {
            auto* visibleColumn = table->GetColumn(0);
            auto* countColumn = table->GetColumn(1);
            auto* typeColumn = table->GetColumn(2);
            auto* colorColumn = table->GetColumn(3);
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
            const int typeWidth = std::max(120, tableWidth - visibleWidth - countWidth - colorWidth - 8);

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

        auto* countColumn = table->GetColumn(0);
        auto* typeColumn = table->GetColumn(1);
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
    };

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
    applyInitialColumnWidths();
    CallAfter(applyInitialColumnWidths);

    table->Bind(wxEVT_SIZE, [applyInitialColumnWidths](wxSizeEvent& evt) {
        applyInitialColumnWidths();
        evt.Skip();
    });
    Bind(wxEVT_SHOW, [applyInitialColumnWidths](wxShowEvent& evt) {
        if (evt.IsShown())
            applyInitialColumnWidths();
        evt.Skip();
    });

    table->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED,
                &SummaryPanel::OnItemValueChanged, this);
    table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                &SummaryPanel::OnItemActivated, this);
    table->Bind(wxEVT_MOTION, &SummaryPanel::OnMouseMove, this);
    table->Bind(wxEVT_LEAVE_WINDOW, &SummaryPanel::OnMouseLeave, this);

}

SummaryPanel* SummaryPanel::Instance()
{
    return s_instance;
}

void SummaryPanel::SetInstance(SummaryPanel* panel)
{
    s_instance = panel;
}

void SummaryPanel::ShowSummary(const std::vector<std::pair<std::string,int>>& items)
{
    if (!table || !store) return;
    EnsureColumnsForMode(SummaryMode::Generic);
    store->DeleteAllItems();
    for (const auto& [name, count] : items) {
        wxVector<wxVariant> row;
        row.push_back(wxString::Format("%d", count));
        row.push_back(wxString::FromUTF8(name));
        store->AppendItem(row);
    }
}

void SummaryPanel::ShowFixtureSummary()
{
    if (!table || !store) return;
    std::map<std::string, FixtureSummaryRow> grouped;
    const auto& fixtures = (*visibilityConfigManager).GetScene().fixtures;
    for (const auto& [uuid, fix] : fixtures) {
        (void)uuid;
        auto& row = grouped[fix.typeName];
        row.typeName = fix.typeName;
        row.count += 1;
        if (row.colorHex.empty() && !fix.color.empty())
            row.colorHex = fix.color;
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

void SummaryPanel::ShowTrussSummary()
{
    std::map<std::string,int> counts;
    const auto& trusses = (*visibilityConfigManager).GetScene().trusses;
    for (const auto& [uuid, truss] : trusses)
        counts[truss.model]++;
    std::vector<std::pair<std::string,int>> items(counts.begin(), counts.end());
    ShowSummary(items);
}

void SummaryPanel::ShowHoistSummary()
{
    if (!table) return;

    std::map<std::string, int> hoistCounts;
    const auto& supports = (*visibilityConfigManager).GetScene().supports;
    for (const auto& [uuid, support] : supports) {
        std::string type = support.function.empty() ? "Hoist" : support.function;
        hoistCounts[type]++;
    }
    std::vector<std::pair<std::string, int>> hoistItems(hoistCounts.begin(), hoistCounts.end());

    ShowSummary(hoistItems);
}

void SummaryPanel::ShowSceneObjectSummary()
{
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
        if (!rowData.colorHex.empty() && color.Set(wxString::FromUTF8(rowData.colorHex)))
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

    while (table->GetColumnCount() > 0)
        table->DeleteColumn(table->GetColumn(0));

    if (requestedMode == SummaryMode::Fixture) {
        table->AppendToggleColumn("Visible", wxDATAVIEW_CELL_ACTIVATABLE, 70,
                                  wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
        table->AppendTextColumn("Count", wxDATAVIEW_CELL_INERT, 60, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        table->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 150, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        auto* colorRenderer = new wxDataViewIconTextRenderer();
        table->AppendColumn(new wxDataViewColumn(
            "Color", colorRenderer, 3, 80, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE));
    } else {
        table->AppendTextColumn("Count", wxDATAVIEW_CELL_INERT, 60, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        table->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 150, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
    }
    mode = requestedMode;
}

void SummaryPanel::RefreshFixtureVisibilityStyles() {
    if (!table || !store || mode != SummaryMode::Fixture)
        return;

    const auto hiddenFixtureTypes =
        (*visibilityConfigManager).GetHiddenFixtureTypes();
    for (unsigned int row = 0; row < table->GetItemCount(); ++row) {
        store->ClearRowTextColour(row);
        wxString typeName = table->GetTextValue(static_cast<int>(row), 2);
        if (hiddenFixtureTypes.find(typeName.ToStdString()) != hiddenFixtureTypes.end())
            store->SetRowTextColour(row, *wxRED);
    }
    table->Refresh();
}

void SummaryPanel::RefreshVisibleViewers() const {
    if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->UpdateScene(false);
        Viewer2DPanel::Instance()->Refresh();
    }
    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
}

void SummaryPanel::OnItemValueChanged(wxDataViewEvent& event) {
    if (!table || mode != SummaryMode::Fixture || event.GetColumn() != 0)
        return;
    const int row = table->ItemToRow(event.GetItem());
    if (row == wxNOT_FOUND)
        return;

    wxVariant value;
    table->GetValue(value, row, 0);
    const bool visible = value.GetBool();
    const std::string typeName = table->GetTextValue(row, 2).ToStdString();

    auto hiddenFixtureTypes =
        (*visibilityConfigManager).GetHiddenFixtureTypes();
    if (visible)
        hiddenFixtureTypes.erase(typeName);
    else
        hiddenFixtureTypes.insert(typeName);
    (*visibilityConfigManager).SetHiddenFixtureTypes(hiddenFixtureTypes);
    RefreshFixtureVisibilityStyles();
    RefreshVisibleViewers();
}

void SummaryPanel::OnItemActivated(wxDataViewEvent& event) {
    if (!table || mode != SummaryMode::Fixture)
        return;
    const int row = table->ItemToRow(event.GetItem());
    if (row == wxNOT_FOUND || event.GetColumn() != 3)
        return;

    const std::string typeName = table->GetTextValue(row, 2).ToStdString();
    auto& colorCfg = (*colorConfigManager);
    auto& fixtures = colorCfg.GetScene().fixtures;

    wxColourData data;
    for (const auto& [uuid, fixture] : fixtures) {
        (void)uuid;
        if (fixture.typeName == typeName && !fixture.color.empty()) {
            data.SetColour(wxColour(wxString::FromUTF8(fixture.color)));
            break;
        }
    }

    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() != wxID_OK)
        return;

    const wxColour color = dlg.GetColourData().GetColour();
    const std::string hex = wxString::Format("#%02X%02X%02X", color.Red(), color.Green(),
                                             color.Blue()).ToStdString();
    colorCfg.PushUndoState("change fixture type color from summary");
    for (auto& [uuid, fixture] : fixtures) {
        (void)uuid;
        if (fixture.typeName == typeName)
            fixture.color = hex;
    }

    wxBitmap bmp(16, 16);
    wxMemoryDC dc(bmp);
    dc.SetBrush(wxBrush(color));
    dc.SetPen(*wxBLACK_PEN);
    dc.DrawRectangle(0, 0, 16, 16);
    dc.SelectObject(wxNullBitmap);
    table->SetValue(wxVariant(wxDataViewIconText("", bmp)), row, 3);
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
            table->GetValue(value, row, 0);
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
