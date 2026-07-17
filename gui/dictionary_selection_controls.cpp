#include "dictionary_selection_controls.h"

namespace {

// Binds a button click to an optional callback.
void BindButton(wxButton *button, const std::function<void()> &callback) {
  if (!button || !callback)
    return;
  button->Bind(wxEVT_BUTTON, [callback](wxCommandEvent &) { callback(); });
}

} // namespace

// Builds the active dictionary path display and explicit dictionary actions.
DictionarySelectionControls BuildDictionarySelectionControls(
    wxWindow *parent, wxSizer *parentSizer, const wxString &title,
    const std::function<void()> &onOpen, const std::function<void()> &onNew,
    const std::function<void()> &onDuplicate,
    const std::function<void()> &onUseDefault) {
  DictionarySelectionControls controls;
  if (!parent || !parentSizer)
    return controls;

  auto *wrapper = new wxStaticBoxSizer(wxVERTICAL, parent, title);
  auto *row = new wxBoxSizer(wxHORIZONTAL);
  auto *buttonRow = new wxBoxSizer(wxHORIZONTAL);

  controls.activeFileLabel =
      new wxStaticText(parent, wxID_ANY, "Dictionary: -");
  controls.activePathLabel = new wxStaticText(parent, wxID_ANY, "Path: -");
  controls.openButton = new wxButton(parent, wxID_ANY, "Open...");
  controls.newButton = new wxButton(parent, wxID_ANY, "New...");
  controls.duplicateButton =
      new wxButton(parent, wxID_ANY, "Duplicate Current...");
  controls.useDefaultButton = new wxButton(parent, wxID_ANY, "Use Default");

  buttonRow->Add(controls.openButton, 0, wxRIGHT, 5);
  buttonRow->Add(controls.newButton, 0, wxRIGHT, 5);
  buttonRow->Add(controls.duplicateButton, 0, wxRIGHT, 5);
  buttonRow->Add(controls.useDefaultButton, 0);

  row->Add(controls.activeFileLabel, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  row->Add(buttonRow, 0, wxALIGN_CENTER_VERTICAL);

  wrapper->Add(row, 0, wxEXPAND | wxALL, 6);
  wrapper->Add(controls.activePathLabel, 0,
               wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
  parentSizer->Add(wrapper, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

  BindButton(controls.openButton, onOpen);
  BindButton(controls.newButton, onNew);
  BindButton(controls.duplicateButton, onDuplicate);
  BindButton(controls.useDefaultButton, onUseDefault);
  return controls;
}

// Updates the active dictionary file and path labels.
void UpdateDictionarySelectionControls(
    const DictionarySelectionControls &controls, const wxString &fileName,
    const wxString &fullPath) {
  if (controls.activeFileLabel) {
    controls.activeFileLabel->SetLabel(
        fileName.IsEmpty() ? wxString("Dictionary: -")
                           : wxString("Dictionary: ") + fileName);
  }
  if (controls.activePathLabel) {
    controls.activePathLabel->SetLabel(fullPath.IsEmpty()
                                           ? wxString("Path: -")
                                           : wxString("Path: ") + fullPath);
    controls.activePathLabel->SetToolTip(fullPath);
  }
}
