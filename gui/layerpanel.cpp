#include "layerpanel.h"
#include "configmanager.h"
#include "viewer3dpanel.h"
#include <set>

LayerPanel* LayerPanel::s_instance = nullptr;

LayerPanel::LayerPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    list = new wxCheckListBox(this, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(list, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);

    list->Bind(wxEVT_CHECKLISTBOX, &LayerPanel::OnCheck, this);

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
    int idx = 0;
    for (const auto& n : names) {
        list->Append(wxString::FromUTF8(n));
        list->Check(idx, hidden.find(n) == hidden.end());
        ++idx;
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
}

