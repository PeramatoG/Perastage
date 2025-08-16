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
#include "viewer3dpanel.h"
#include "viewer2dpanel.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "sceneobjecttablepanel.h"
#include <set>
#include <chrono>
#include <algorithm>
#include <wx/dcmemory.h>

LayerPanel* LayerPanel::s_instance = nullptr;

LayerPanel::LayerPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    list = new wxDataViewListCtrl(this, wxID_ANY);
    list->AppendToggleColumn("Visible");
    list->AppendTextColumn("Layer");
    auto* colorRenderer = new wxDataViewIconTextRenderer();
    auto* colorColumn = new wxDataViewColumn("Color", colorRenderer, 2, 40, wxALIGN_CENTER);
    list->AppendColumn(colorColumn);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(list, 1, wxEXPAND | wxALL, 5);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* addBtn = new wxButton(this, wxID_ADD, "Add");
    auto* delBtn = new wxButton(this, wxID_DELETE, "Delete");
    btnSizer->Add(addBtn, 0, wxALL, 5);
    btnSizer->Add(delBtn, 0, wxALL, 5);
    sizer->Add(btnSizer, 0, wxALIGN_LEFT);

    SetSizer(sizer);

    list->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, &LayerPanel::OnCheck, this);
    list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &LayerPanel::OnSelect, this);
    list->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &LayerPanel::OnContext, this);
    list->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &LayerPanel::OnRenameLayer, this);
    addBtn->Bind(wxEVT_BUTTON, &LayerPanel::OnAddLayer, this);
    delBtn->Bind(wxEVT_BUTTON, &LayerPanel::OnDeleteLayer, this);

    ReloadLayers();
}

LayerPanel* LayerPanel::Instance()
{
    return s_instance;
}

void LayerPanel::SetInstance(LayerPanel* p)
{
    s_instance = p;
}

void LayerPanel::ReloadLayers()
{
    if (!list) return;
    list->DeleteAllItems();

    std::set<std::string> names;
    auto& scene = ConfigManager::Get().GetScene();
    for (const auto& [uuid, layer] : scene.layers)
        names.insert(layer.name);

    auto collect = [&](const std::string& ln) {
        if (!ln.empty())
            names.insert(ln);
    };
    for (const auto& [u,f] : scene.fixtures) collect(f.layer);
    for (const auto& [u,t] : scene.trusses) collect(t.layer);
    for (const auto& [u,o] : scene.sceneObjects) collect(o.layer);
    names.insert(DEFAULT_LAYER_NAME);

    auto hidden = ConfigManager::Get().GetHiddenLayers();
    std::string current = ConfigManager::Get().GetCurrentLayer();
    int idx = 0;
    int sel = -1;

    auto addRow = [&](const std::string& n){
        bool vis = hidden.find(n) == hidden.end();
        wxVector<wxVariant> cols;
        cols.push_back(wxVariant(vis));
        cols.push_back(wxVariant(wxString::FromUTF8(n)));
        wxBitmap bmp(16,16);
        wxColour c;
        auto opt = ConfigManager::Get().GetLayerColor(n);
        if (opt)
            c.Set(wxString::FromUTF8(opt->c_str()));
        else
            c.Set(128,128,128);
        wxMemoryDC dc(bmp);
        dc.SetBrush(wxBrush(c));
        dc.SetPen(*wxBLACK_PEN);
        dc.DrawRectangle(0,0,16,16);
        dc.SelectObject(wxNullBitmap);
        wxDataViewIconText icon("", bmp);
        cols.push_back(wxVariant(icon));
        list->AppendItem(cols);
        if (n == current)
            sel = idx;
        ++idx;
    };

    if (names.find(DEFAULT_LAYER_NAME) != names.end()) {
        addRow(DEFAULT_LAYER_NAME);
        names.erase(DEFAULT_LAYER_NAME);
    }
    for (const auto& n : names)
        addRow(n);

    if (sel < 0 && list->GetItemCount() > 0)
        sel = 0;
    if (sel >= 0) {
        list->SelectRow(sel);
        wxString wname = list->GetTextValue(sel,1);
        ConfigManager::Get().SetCurrentLayer(wname.ToStdString());
    }
}

void LayerPanel::OnCheck(wxDataViewEvent& evt)
{
    unsigned int idx = list->ItemToRow(evt.GetItem());
    wxString wname = list->GetTextValue(idx,1);
    std::string name = wname.ToStdString();
    wxVariant v;
    list->GetValue(v, idx, 0);
    bool checked = v.GetBool();
    auto hidden = ConfigManager::Get().GetHiddenLayers();
    if (checked)
        hidden.erase(name);
    else
        hidden.insert(name);
    ConfigManager::Get().SetHiddenLayers(hidden);
    if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->Refresh();
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->Refresh();
}

void LayerPanel::OnSelect(wxDataViewEvent& evt)
{
    unsigned int idx = list->ItemToRow(evt.GetItem());
    if (idx == wxNOT_FOUND)
        return;
    wxString wname = list->GetTextValue(idx,1);
    ConfigManager::Get().SetCurrentLayer(wname.ToStdString());
}

void LayerPanel::OnContext(wxDataViewEvent& evt)
{
    unsigned int idx = list->ItemToRow(evt.GetItem());
    if (idx == wxNOT_FOUND)
        return;
    wxString wname = list->GetTextValue(idx,1);
    std::string name = wname.ToStdString();
    wxColourData data;
    if (auto c = ConfigManager::Get().GetLayerColor(name))
        data.SetColour(wxColour(wxString::FromUTF8(c->c_str())));
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() != wxID_OK)
        return;
    wxColour col = dlg.GetColourData().GetColour();
    std::string hex = wxString::Format("#%02X%02X%02X", col.Red(), col.Green(), col.Blue()).ToStdString();
    ConfigManager::Get().PushUndoState("change layer color");
    ConfigManager::Get().SetLayerColor(name, hex);
    wxBitmap bmp(16,16);
    wxMemoryDC dc(bmp);
    dc.SetBrush(wxBrush(col));
    dc.SetPen(*wxBLACK_PEN);
    dc.DrawRectangle(0,0,16,16);
    dc.SelectObject(wxNullBitmap);
    wxDataViewIconText icon("", bmp);
    wxVariant vv(icon);
    list->SetValue(vv, idx, 2);
    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->SetLayerColor(name, hex);
        Viewer3DPanel::Instance()->Refresh();
    }
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->Refresh();
}

void LayerPanel::OnAddLayer(wxCommandEvent&)
{
    wxTextEntryDialog dlg(this, "Enter new layer name:", "Add Layer");
    if (dlg.ShowModal() != wxID_OK)
        return;
    std::string name = dlg.GetValue().ToStdString();
    if (name.empty() || name == DEFAULT_LAYER_NAME)
        return;

    ConfigManager& cfg = ConfigManager::Get();
    auto& scene = cfg.GetScene();
    for (const auto& [uuid, layer] : scene.layers)
        if (layer.name == name)
        {
            wxMessageBox("Layer already exists.", "Add Layer", wxOK | wxICON_ERROR, this);
            return;
        }

    cfg.PushUndoState("add layer");
    Layer layer;
    auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId)).ToStdString();
    layer.name = name;
    scene.layers[layer.uuid] = layer;
    cfg.SetCurrentLayer(name);
    ReloadLayers();
}

void LayerPanel::OnDeleteLayer(wxCommandEvent&)
{
    if (!list)
        return;
    int sel = list->GetSelectedRow();
    if (sel == wxNOT_FOUND)
        return;
    wxString wname = list->GetTextValue(sel,1);
    std::string name = wname.ToStdString();
    if (name == DEFAULT_LAYER_NAME)
    {
        wxMessageBox("Cannot delete default layer.", "Delete Layer", wxOK | wxICON_ERROR, this);
        return;
    }

    ConfigManager& cfg = ConfigManager::Get();
    auto& scene = cfg.GetScene();
    std::string layerUuid;
    for (const auto& [uuid, layer] : scene.layers)
        if (layer.name == name)
        {
            layerUuid = uuid;
            break;
        }
    if (layerUuid.empty())
        return;

    bool empty = true;
    for (const auto& [u, f] : scene.fixtures)
        if (f.layer == name) { empty = false; break; }
    if (empty)
        for (const auto& [u, t] : scene.trusses)
            if (t.layer == name) { empty = false; break; }
    if (empty)
        for (const auto& [u, o] : scene.sceneObjects)
            if (o.layer == name) { empty = false; break; }

    if (!empty)
    {
        int res = wxMessageBox("Layer is not empty. Delete all elements?",
                               "Delete Layer", wxYES_NO | wxICON_WARNING, this);
        if (res != wxYES)
            return;
    }

    cfg.PushUndoState("delete layer");

    for (auto it = scene.fixtures.begin(); it != scene.fixtures.end();) {
        if (it->second.layer == name)
            it = scene.fixtures.erase(it);
        else
            ++it;
    }
    for (auto it = scene.trusses.begin(); it != scene.trusses.end();) {
        if (it->second.layer == name)
            it = scene.trusses.erase(it);
        else
            ++it;
    }
    for (auto it = scene.sceneObjects.begin(); it != scene.sceneObjects.end();) {
        if (it->second.layer == name)
            it = scene.sceneObjects.erase(it);
        else
            ++it;
    }

    scene.layers.erase(layerUuid);

    auto hidden = cfg.GetHiddenLayers();
    hidden.erase(name);
    cfg.SetHiddenLayers(hidden);
    if (cfg.GetCurrentLayer() == name)
        cfg.SetCurrentLayer(DEFAULT_LAYER_NAME);

    auto selFix = cfg.GetSelectedFixtures();
    selFix.erase(std::remove_if(selFix.begin(), selFix.end(),
        [&](const std::string& u){ return scene.fixtures.find(u) == scene.fixtures.end(); }), selFix.end());
    cfg.SetSelectedFixtures(selFix);
    auto selTr = cfg.GetSelectedTrusses();
    selTr.erase(std::remove_if(selTr.begin(), selTr.end(),
        [&](const std::string& u){ return scene.trusses.find(u) == scene.trusses.end(); }), selTr.end());
    cfg.SetSelectedTrusses(selTr);
    auto selObj = cfg.GetSelectedSceneObjects();
    selObj.erase(std::remove_if(selObj.begin(), selObj.end(),
        [&](const std::string& u){ return scene.sceneObjects.find(u) == scene.sceneObjects.end(); }), selObj.end());
    cfg.SetSelectedSceneObjects(selObj);

    ReloadLayers();
    if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->ReloadData();
    if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->ReloadData();
    if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->ReloadData();
    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
}

void LayerPanel::OnRenameLayer(wxDataViewEvent& evt)
{
    if (!list)
        return;
    unsigned int idx = list->ItemToRow(evt.GetItem());
    if (idx == wxNOT_FOUND)
        return;

    wxString oldW = list->GetTextValue(idx,1);
    std::string oldName = oldW.ToStdString();
    if (oldName == DEFAULT_LAYER_NAME)
    {
        wxMessageBox("Cannot rename default layer.", "Rename Layer", wxOK | wxICON_ERROR, this);
        return;
    }

    wxTextEntryDialog dlg(this, "Enter new layer name:", "Rename Layer", oldW);
    if (dlg.ShowModal() != wxID_OK)
        return;
    std::string newName = dlg.GetValue().ToStdString();
    if (newName.empty() || newName == DEFAULT_LAYER_NAME || newName == oldName)
        return;

    ConfigManager& cfg = ConfigManager::Get();
    auto& scene = cfg.GetScene();

    // check duplicate
    for (const auto& [u, layer] : scene.layers)
        if (layer.name == newName)
        {
            wxMessageBox("Layer already exists.", "Rename Layer", wxOK | wxICON_ERROR, this);
            return;
        }

    std::string layerUuid;
    for (const auto& [uuid, layer] : scene.layers)
        if (layer.name == oldName)
        {
            layerUuid = uuid;
            break;
        }
    if (layerUuid.empty())
        return;

    cfg.PushUndoState("rename layer");

    scene.layers[layerUuid].name = newName;
    auto rename = [&](auto& container){
        for (auto& [u, obj] : container)
            if (obj.layer == oldName)
                obj.layer = newName;
    };
    rename(scene.fixtures);
    rename(scene.trusses);
    rename(scene.sceneObjects);

    auto hidden = cfg.GetHiddenLayers();
    if (hidden.erase(oldName))
        hidden.insert(newName);
    cfg.SetHiddenLayers(hidden);
    if (cfg.GetCurrentLayer() == oldName)
        cfg.SetCurrentLayer(newName);

    ReloadLayers();
    if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->ReloadData();
    if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->ReloadData();
    if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->ReloadData();
    if (Viewer3DPanel::Instance()) {
        Viewer3DPanel::Instance()->UpdateScene();
        Viewer3DPanel::Instance()->Refresh();
    }
}

