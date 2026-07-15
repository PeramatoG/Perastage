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
#include "layerpanel.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "layer_service.h"
#include "wx_text_utils.h"
#include "fixturetablepanel.h"
#include "sceneobjecttablepanel.h"
#include "table_column_indices.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <set>
#include <algorithm>
#include <functional>
#include <wx/dcmemory.h>

LayerPanel* LayerPanel::s_instance = nullptr;

namespace {
using LayerColumn = LayerTableColumns::Column;

// Converts a layer column to its stable model index.
constexpr int ColumnIndex(LayerColumn column) {
    return TableColumnIndices::ToIndex(column);
}

// Refreshes open viewer panels after layer visibility or color changes.
void RefreshVisibleViewers() {
    std::function<void(wxWindow*)> visit;
    visit = [&](wxWindow* window) {
        if (!window)
            return;

        if (auto* viewer3d = dynamic_cast<Viewer3DPanel*>(window)) {
            if (viewer3d->IsShownOnScreen()) {
                viewer3d->UpdateScene();
                viewer3d->Refresh();
            }
        }

        if (auto* viewer2d = dynamic_cast<Viewer2DPanel*>(window)) {
            if (viewer2d->IsShownOnScreen()) {
                // Hidden-layer visibility is read from ConfigManager during
                // draw calls, so forcing repaint is enough to apply toggles
                // immediately even when heavy reload work is paused.
                viewer2d->UpdateScene(false);
                viewer2d->Refresh();
            }
        }

        for (auto* child : window->GetChildren())
            visit(child);
    };

    for (auto* top : wxTopLevelWindows)
        visit(top);
}
} // namespace

// Builds the layer list panel and wires layer management events.
LayerPanel::LayerPanel(wxWindow* parent, bool showButtons, ConfigManager* config)
    : wxPanel(parent, wxID_ANY), configManager(config ? config : &GetDefaultGuiConfigServices().LegacyConfigManager())
{
    list = new wxDataViewListCtrl(this, wxID_ANY);
    auto* visibleColumn = list->AppendToggleColumn(_("Visible"));
    auto* layerColumn = list->AppendTextColumn(_("Layer"));
    auto* colorRenderer = new wxDataViewIconTextRenderer();
    auto* colorColumn = new wxDataViewColumn(_("Color"), colorRenderer,
                                             ColumnIndex(LayerColumn::Color), 40, wxALIGN_CENTER,
                                             wxDATAVIEW_COL_RESIZABLE);
    list->AppendColumn(colorColumn);

    auto applyInitialColumnWidths = [this, visibleColumn, layerColumn, colorColumn]() {
        if (!list || !visibleColumn || !layerColumn || !colorColumn)
            return;

        wxClientDC dc(list);
        dc.SetFont(list->GetFont());
        int visibleLabelWidth = 0;
        int colorLabelWidth = 0;
        dc.GetTextExtent(_("Visible"), &visibleLabelWidth, nullptr);
        dc.GetTextExtent(_("Color"), &colorLabelWidth, nullptr);

    const int visibleWidth =
        visibleLabelWidth + 28; // checkbox + header padding
    const int colorWidth =
        std::max(colorLabelWidth + 16, 16 + 20); // label or swatch
        const int listWidth = std::max(0, list->GetClientSize().GetWidth());
    const int layerWidth =
        std::max(120, listWidth - visibleWidth - colorWidth - 8);

        visibleColumn->SetMinWidth(visibleWidth);
        visibleColumn->SetWidth(visibleWidth);
        colorColumn->SetMinWidth(colorWidth);
        colorColumn->SetWidth(colorWidth);
        layerColumn->SetMinWidth(120);
        layerColumn->SetWidth(layerWidth);
    };

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(list, 1, wxEXPAND | wxALL, 5);

    wxButton* addBtn = nullptr;
    wxButton* delBtn = nullptr;
    if (showButtons) {
        wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
        addBtn = new wxButton(this, wxID_ADD, _("Add"));
        delBtn = new wxButton(this, wxID_DELETE, _("Delete"));
        btnSizer->Add(addBtn, 0, wxALL, 5);
        btnSizer->Add(delBtn, 0, wxALL, 5);
        sizer->Add(btnSizer, 0, wxALIGN_LEFT);
    }

    SetSizer(sizer);
    applyInitialColumnWidths();
    CallAfter(applyInitialColumnWidths);

    list->Bind(wxEVT_SIZE, [applyInitialColumnWidths](wxSizeEvent& evt) {
        applyInitialColumnWidths();
        evt.Skip();
    });
    Bind(wxEVT_SHOW, [applyInitialColumnWidths](wxShowEvent& evt) {
        if (evt.IsShown())
            applyInitialColumnWidths();
        evt.Skip();
    });

    list->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, &LayerPanel::OnCheck, this);
    list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &LayerPanel::OnSelect, this);
    list->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &LayerPanel::OnContext, this);
    list->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &LayerPanel::OnRenameLayer, this);
    if (addBtn)
        addBtn->Bind(wxEVT_BUTTON, &LayerPanel::OnAddLayer, this);
    if (delBtn)
        delBtn->Bind(wxEVT_BUTTON, &LayerPanel::OnDeleteLayer, this);

    ReloadLayers();
}

// Returns the globally registered layer panel instance.
LayerPanel *LayerPanel::Instance() { return s_instance; }

// Stores the globally registered layer panel instance.
void LayerPanel::SetInstance(LayerPanel *p) { s_instance = p; }

// Rebuilds the visible layer rows from the current scene and visibility state.
void LayerPanel::ReloadLayers() {
  if (!list)
    return;
  list->DeleteAllItems();
  rowLayerUuids.clear();

  auto &scene = (*configManager).GetScene();
  auto hidden = (*configManager).GetHiddenLayers();
  std::string current = (*configManager).GetCurrentLayer();
  int idx = 0;
  int sel = -1;

  auto addRow = [&](const layerdomain::LayerEntry &entry) {
    const bool vis = hidden.find(entry.name) == hidden.end();
    wxVector<wxVariant> cols;
    cols.push_back(wxVariant(vis));
    cols.push_back(wxVariant(wxtext::FromUtf8(entry.name)));
    wxBitmap bmp(16, 16);
    wxColour c;
    if (!entry.color.empty())
      c.Set(wxtext::FromUtf8(entry.color));
    else
      c.Set(128, 128, 128);
    wxMemoryDC dc(bmp);
    dc.SetBrush(wxBrush(c));
    dc.SetPen(*wxBLACK_PEN);
    dc.DrawRectangle(0, 0, 16, 16);
    dc.SelectObject(wxNullBitmap);
    wxDataViewIconText icon("", bmp);
    cols.push_back(wxVariant(icon));
    list->AppendItem(cols);
    rowLayerUuids.push_back(entry.uuid);
    if (entry.name == current)
      sel = idx;
    ++idx;
  };

  for (const auto &entry : layerdomain::EnumerateLayers(scene))
    addRow(entry);

  if (sel < 0 && list->GetItemCount() > 0)
    sel = 0;
  if (sel >= 0) {
    list->SelectRow(sel);
    const std::string uuid = LayerUuidForRow(sel);
    if (!uuid.empty())
      layerdomain::SetCurrentLayer(*configManager, uuid);
  }
}

// Returns the stable layer UUID bound to a visible row.
std::string LayerPanel::LayerUuidForRow(int row) const {
  if (row < 0 || row >= static_cast<int>(rowLayerUuids.size()))
    return {};
  return rowLayerUuids[static_cast<size_t>(row)];
}

// Applies visibility checkbox changes to the hidden-layer set.
void LayerPanel::OnCheck(wxDataViewEvent &evt) {
    int idx = static_cast<int>(list->ItemToRow(evt.GetItem()));
  if (idx == wxNOT_FOUND || idx < 0 ||
      idx >= static_cast<int>(list->GetItemCount()))
        return;
  const std::string uuid = LayerUuidForRow(idx);
    if (uuid.empty())
        return;
    wxVariant v;
  list->GetValue(v, idx, ColumnIndex(LayerColumn::Visible));
    bool checked = v.GetBool();
    auto result = layerdomain::SetLayerVisibility(*configManager, uuid, checked);
    if (result.status == layerdomain::LayerStatus::Success)
        RefreshVisibleViewers();
}

// Updates the current layer when the selected row changes.
void LayerPanel::OnSelect(wxDataViewEvent &evt) {
    unsigned int idx = list->ItemToRow(evt.GetItem());
    if (idx == wxNOT_FOUND)
        return;
  const std::string uuid = LayerUuidForRow(static_cast<int>(idx));
    if (!uuid.empty())
        layerdomain::SetCurrentLayer(*configManager, uuid);
}

// Opens the layer color picker for the context-menu row.
void LayerPanel::OnContext(wxDataViewEvent &evt) {
    unsigned int idx = list->ItemToRow(evt.GetItem());
    if (idx == wxNOT_FOUND)
        return;
  const std::string uuid = LayerUuidForRow(static_cast<int>(idx));
    if (uuid.empty())
        return;
    const auto layerIt = (*configManager).GetScene().layers.find(uuid);
    if (layerIt == (*configManager).GetScene().layers.end())
        return;
    const std::string name = layerIt->second.name;
    wxColourData data;
    if (!layerIt->second.color.empty())
        data.SetColour(wxColour(wxtext::FromUtf8(layerIt->second.color)));
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() != wxID_OK)
        return;
    wxColour col = dlg.GetColourData().GetColour();
  std::string hex = wxtext::ToUtf8(
      wxString::Format("#%02X%02X%02X", col.Red(), col.Green(), col.Blue()));
    auto result = layerdomain::SetLayerColor(*configManager, uuid, hex);
    if (result.status != layerdomain::LayerStatus::Success &&
        result.status != layerdomain::LayerStatus::NoChange) {
        wxMessageBox(wxtext::FromUtf8(result.message.empty() ? layerdomain::StatusMessage(result.status) : result.message),
                     _("Layer Color"), wxOK | wxICON_ERROR, this);
        return;
    }
    wxBitmap bmp(16,16);
    wxMemoryDC dc(bmp);
    dc.SetBrush(wxBrush(col));
    dc.SetPen(*wxBLACK_PEN);
    dc.DrawRectangle(0,0,16,16);
    dc.SelectObject(wxNullBitmap);
    wxDataViewIconText icon("", bmp);
    wxVariant vv(icon);
  list->SetValue(vv, idx, ColumnIndex(LayerColumn::Color));
    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->SetLayerColor(name, hex);
        Viewer3DPanel::Instance()->Refresh();
    }
    if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->SetLayerColor(name, hex);
        Viewer2DPanel::Instance()->Refresh();
    }
}

// Adds a new named layer to the scene.
void LayerPanel::OnAddLayer(wxCommandEvent &) {
    wxTextEntryDialog dlg(this, _("Enter new layer name:"), _("Add Layer"));
    if (dlg.ShowModal() != wxID_OK)
        return;
    auto result = layerdomain::CreateLayer(*configManager, wxtext::ToUtf8(dlg.GetValue()));
    if (result.status != layerdomain::LayerStatus::Success &&
        result.status != layerdomain::LayerStatus::NoChange) {
        wxMessageBox(wxtext::FromUtf8(result.message.empty() ? layerdomain::StatusMessage(result.status) : result.message),
                     _("Add Layer"), wxOK | wxICON_ERROR, this);
        return;
    }
    ReloadLayers();
}

// Deletes the selected non-default layer and any contained elements when confirmed.
void LayerPanel::OnDeleteLayer(wxCommandEvent&)
{
    if (!list)
        return;
    int sel = list->GetSelectedRow();
    if (sel == wxNOT_FOUND)
        return;
    const std::string uuid = LayerUuidForRow(sel);
    auto &scene = (*configManager).GetScene();
    auto layerIt = scene.layers.find(uuid);
    if (uuid.empty() || layerIt == scene.layers.end()) {
        wxMessageBox(_("Layer no longer exists."), _("Delete Layer"), wxOK | wxICON_ERROR, this);
        return;
    }
    const std::string name = layerIt->second.name;
    if (name == DEFAULT_LAYER_NAME)
    {
        wxMessageBox(_("Cannot delete default layer."), _("Delete Layer"), wxOK | wxICON_ERROR, this);
        return;
    }

    bool empty = true;
    auto contains = [&](const auto &container) {
        for (const auto &[u, obj] : container) {
            (void)u;
            if (obj.layer == name)
                return true;
        }
        return false;
    };
    empty = !(contains(scene.fixtures) || contains(scene.trusses) ||
              contains(scene.sceneObjects) || contains(scene.supports) ||
              contains(scene.groupObjects));

    if (!empty)
    {
        int res = wxMessageBox(_("Layer is not empty. Delete all elements?"),
                               _("Delete Layer"), wxYES_NO | wxICON_WARNING, this);
        if (res != wxYES)
            return;
    }

    if (scene.layers.find(uuid) == scene.layers.end()) {
        wxMessageBox(_("Layer no longer exists."), _("Delete Layer"), wxOK | wxICON_ERROR, this);
        return;
    }
    auto result = layerdomain::DeleteLayer(*configManager, uuid);
    if (result.status != layerdomain::LayerStatus::Success) {
        wxMessageBox(wxtext::FromUtf8(result.message.empty() ? layerdomain::StatusMessage(result.status) : result.message),
                     _("Delete Layer"), wxOK | wxICON_ERROR, this);
        return;
    }

    ReloadLayers();
    if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->ReloadData();
    if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->ReloadData();
    if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->ReloadData();
    if (HoistTablePanel::Instance())
        HoistTablePanel::Instance()->ReloadData();
    RefreshVisibleViewers();
}

// Renames the activated layer when the layer-name column is activated.
void LayerPanel::OnRenameLayer(wxDataViewEvent &evt) {
    if (!list)
        return;
    if (evt.GetColumn() != ColumnIndex(LayerColumn::Layer)) {
        evt.Skip();
        return;
    }
    unsigned int idx = list->ItemToRow(evt.GetItem());
    if (idx == wxNOT_FOUND)
        return;

    const std::string uuid = LayerUuidForRow(static_cast<int>(idx));
    auto &scene = (*configManager).GetScene();
    auto layerIt = scene.layers.find(uuid);
    if (uuid.empty() || layerIt == scene.layers.end())
        return;
    const std::string oldName = layerIt->second.name;
    if (oldName == DEFAULT_LAYER_NAME) {
        wxMessageBox(_("Cannot rename default layer."), _("Rename Layer"),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    wxTextEntryDialog dlg(this, _("Enter new layer name:"), _("Rename Layer"), wxtext::FromUtf8(oldName));
    if (dlg.ShowModal() != wxID_OK)
        return;
    auto result = layerdomain::RenameLayer(*configManager, uuid, wxtext::ToUtf8(dlg.GetValue()));
    if (result.status == layerdomain::LayerStatus::NoChange)
        return;
    if (result.status != layerdomain::LayerStatus::Success) {
        wxMessageBox(wxtext::FromUtf8(result.message.empty() ? layerdomain::StatusMessage(result.status) : result.message),
                     _("Rename Layer"), wxOK | wxICON_ERROR, this);
        return;
    }

    ReloadLayers();
    if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->ReloadData();
    if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->ReloadData();
    if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->ReloadData();
    if (HoistTablePanel::Instance())
        HoistTablePanel::Instance()->ReloadData();
    RefreshVisibleViewers();
}
