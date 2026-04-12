#include "dictionary_selection_controls.h"

#include <wx/button.h>

DictionarySelectionControls BuildDictionarySelectionControls(
    wxWindow *parent, wxSizer *parentSizer, const wxString &title,
    const wxString &buttonLabel, const std::function<void()> &onSelect) {
  DictionarySelectionControls controls;
  if (!parent || !parentSizer)
    return controls;

  auto *wrapper = new wxStaticBoxSizer(wxVERTICAL, parent, title);
  auto *row = new wxBoxSizer(wxHORIZONTAL);

  controls.activeFileLabel = new wxStaticText(parent, wxID_ANY, "Dictionary: -");
  controls.activePathLabel = new wxStaticText(parent, wxID_ANY, "Path: -");
  controls.selectButton = new wxButton(parent, wxID_ANY, buttonLabel);

  row->Add(controls.activeFileLabel, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  row->Add(controls.selectButton, 0, wxALIGN_CENTER_VERTICAL);

  wrapper->Add(row, 0, wxEXPAND | wxALL, 6);
  wrapper->Add(controls.activePathLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
               6);
  parentSizer->Add(wrapper, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

  if (controls.selectButton && onSelect) {
    controls.selectButton->Bind(wxEVT_BUTTON,
                                [onSelect](wxCommandEvent &) { onSelect(); });
  }
  return controls;
}

void UpdateDictionarySelectionControls(const DictionarySelectionControls &controls,
                                       const wxString &fileName,
                                       const wxString &fullPath) {
  if (controls.activeFileLabel) {
    controls.activeFileLabel->SetLabel(
        fileName.IsEmpty() ? wxString("Dictionary: -")
                           : wxString("Dictionary: ") + fileName);
  }
  if (controls.activePathLabel) {
    controls.activePathLabel->SetLabel(
        fullPath.IsEmpty() ? wxString("Path: -")
                           : wxString("Path: ") + fullPath);
    controls.activePathLabel->SetToolTip(fullPath);
  }
}
