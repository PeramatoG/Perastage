#include "gdtf_wheel_inspector_panel.h"

#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

// Creates the read-only wheel and slot inspector panel.
GdtfWheelInspectorPanel::GdtfWheelInspectorPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(this, wxID_ANY, "Active DMX mapping"), 0, wxBOTTOM, 3);
  activeTextCtrl = new wxTextCtrl(this, wxID_ANY, wxString(), wxDefaultPosition,
                                  wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
  root->Add(activeTextCtrl, 1, wxEXPAND | wxBOTTOM, 6);
  root->Add(new wxStaticText(this, wxID_ANY, "Wheel slots"), 0, wxBOTTOM, 3);
  slotList = new wxListBox(this, wxID_ANY);
  root->Add(slotList, 1, wxEXPAND);
  SetSizer(root);
  ClearPresentation();
}

// Applies the current read-only wheel inspection presentation.
void GdtfWheelInspectorPanel::SetPresentation(
    const GdtfWheelInspectorPresentation &presentation) {
  activeTextCtrl->SetValue(wxString::FromUTF8(presentation.activeText));
  slotList->Clear();
  int selectedIndex = wxNOT_FOUND;
  for (size_t i = 0; i < presentation.slots.size(); ++i) {
    slotList->Append(wxString::FromUTF8(presentation.slots[i].label));
    if (presentation.slots[i].selected)
      selectedIndex = static_cast<int>(i);
  }
  if (selectedIndex != wxNOT_FOUND)
    slotList->SetSelection(selectedIndex);
}

// Clears the wheel inspector to an unavailable read-only state.
void GdtfWheelInspectorPanel::ClearPresentation() {
  activeTextCtrl->SetValue("Select a DMX channel and move the inspection slider to resolve the active function, set, wheel, and slot.");
  slotList->Clear();
}
