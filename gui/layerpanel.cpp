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

LayerPanel* LayerPanel::s_instance = nullptr;

LayerPanel::LayerPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    list = new wxCheckListBox(this, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(list, 1, wxEXPAND | wxALL, 5);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* addBtn = new wxButton(this, wxID_ADD, "Add");
    auto* delBtn = new wxButton(this, wxID_DELETE, "Delete");
    btnSizer->Add(addBtn, 0, wxALL, 5);
    btnSizer->Add(delBtn, 0, wxALL, 5);
    sizer->Add(btnSizer, 0, wxALIGN_LEFT);

    SetSizer(sizer);

    list->Bind(wxEVT_CHECKLISTBOX, &LayerPanel::OnCheck, this);
    list->Bind(wxEVT_LISTBOX, &LayerPanel::OnSelect, this);
    list->Bind(wxEVT_LISTBOX_DCLICK, &LayerPanel::OnRenameLayer, this);
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
    list->Clear();

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
    if (names.find(DEFAULT_LAYER_NAME) != names.end()) {
        list->Append(wxString::FromUTF8(DEFAULT_LAYER_NAME));
        list->Check(idx, hidden.find(DEFAULT_LAYER_NAME) == hidden.end());
        if (current == DEFAULT_LAYER_NAME)
            sel = idx;
        ++idx;
    }
    for (const auto& n : names) {
        if (n == DEFAULT_LAYER_NAME)
            continue;
        list->Append(wxString::FromUTF8(n));
        list->Check(idx, hidden.find(n) == hidden.end());
        if (n == current)
            sel = idx;
        ++idx;
    }
    if (sel < 0 && list->GetCount() > 0)
        sel = 0;
    if (sel >= 0) {
        list->SetSelection(sel);
        wxString wname = list->GetString(sel);
        ConfigManager::Get().SetCurrentLayer(wname.ToStdString());
    }
}

void LayerPanel::OnCheck(wxCommandEvent& evt)
{
    int idx = evt.GetInt();
    wxString wname = list->GetString(idx);
    std::string name = wname.ToStdString();
    auto hidden = ConfigManager::Get().GetHiddenLayers();
    if (list->IsChecked(idx))
        hidden.erase(name);
    else
        hidden.insert(name);
    ConfigManager::Get().SetHiddenLayers(hidden);
    if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->Refresh();
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->Refresh();
}

void LayerPanel::OnSelect(wxCommandEvent& evt)
{
    int idx = evt.GetInt();
    if (idx >= 0 && idx < static_cast<int>(list->GetCount())) {
        wxString wname = list->GetString(idx);
        ConfigManager::Get().SetCurrentLayer(wname.ToStdString());
    }
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
    int sel = list->GetSelection();
    if (sel == wxNOT_FOUND)
        return;
    wxString wname = list->GetString(sel);
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

void LayerPanel::OnRenameLayer(wxCommandEvent& evt)
{
    if (!list)
        return;
    int idx = evt.GetInt();
    if (idx == wxNOT_FOUND)
        return;

    wxString oldW = list->GetString(idx);
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

