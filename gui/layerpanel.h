#pragma once

#include <wx/wx.h>
#include <wx/checklst.h>

class LayerPanel : public wxPanel
{
public:
    explicit LayerPanel(wxWindow* parent);
    void ReloadLayers();

    static LayerPanel* Instance();
    static void SetInstance(LayerPanel* p);

private:
    void OnCheck(wxCommandEvent& evt);
    wxCheckListBox* list = nullptr;
    static LayerPanel* s_instance;
};

