#include "dictionary_selection_controls.h"

namespace {

constexpr int kDuplicateCurrentMenuId = wxID_HIGHEST + 410;
constexpr int kUseDefaultMenuId = wxID_HIGHEST + 411;
constexpr int kResetContentsMenuId = wxID_HIGHEST + 412;

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
    const std::function<void()> &onUseDefault,
    const std::function<void()> &onReset) {
  DictionarySelectionControls controls;
  if (!parent || !parentSizer)
    return controls;

  auto *wrapper = new wxStaticBoxSizer(wxVERTICAL, parent, title);
  auto *row = new wxBoxSizer(wxHORIZONTAL);
  auto *buttonRow = new wxBoxSizer(wxHORIZONTAL);

  controls.activeFileLabel =
      new wxStaticText(parent, wxID_ANY, "Dictionary: -", wxDefaultPosition,
                       wxDefaultSize, wxST_ELLIPSIZE_MIDDLE);
  controls.activePathLabel =
      new wxStaticText(parent, wxID_ANY, "Path: -", wxDefaultPosition,
                       wxDefaultSize, wxST_ELLIPSIZE_MIDDLE);
  controls.openButton = new wxButton(parent, wxID_ANY, "Open...");
  controls.newButton = new wxButton(parent, wxID_ANY, "New...");
  controls.moreButton = new wxButton(parent, wxID_ANY, "More...");
  controls.moreMenu = new wxMenu;
  controls.moreMenu->Append(kDuplicateCurrentMenuId, "Duplicate Current...");
  controls.moreMenu->Append(kUseDefaultMenuId, "Use Default");
  controls.moreMenu->AppendSeparator();
  controls.moreMenu->Append(kResetContentsMenuId, "Reset Contents...");

  buttonRow->Add(controls.openButton, 0, wxRIGHT, 5);
  buttonRow->Add(controls.newButton, 0, wxRIGHT, 5);
  buttonRow->Add(controls.moreButton, 0);

  row->Add(controls.activeFileLabel, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  row->Add(buttonRow, 0, wxALIGN_CENTER_VERTICAL);

  wrapper->Add(row, 0, wxEXPAND | wxALL, 6);
  wrapper->Add(controls.activePathLabel, 0,
               wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
  parentSizer->Add(wrapper, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

  BindButton(controls.openButton, onOpen);
  BindButton(controls.newButton, onNew);
  if (controls.moreButton) {
    controls.moreButton->Bind(wxEVT_BUTTON,
                              [button = controls.moreButton,
                               menu = controls.moreMenu](wxCommandEvent &) {
                                if (button && menu)
                                  button->PopupMenu(menu);
                              });
  }
  if (onDuplicate)
    parent->Bind(
        wxEVT_MENU, [onDuplicate](wxCommandEvent &) { onDuplicate(); },
        kDuplicateCurrentMenuId);
  if (onUseDefault)
    parent->Bind(
        wxEVT_MENU, [onUseDefault](wxCommandEvent &) { onUseDefault(); },
        kUseDefaultMenuId);
  if (onReset)
    parent->Bind(
        wxEVT_MENU, [onReset](wxCommandEvent &) { onReset(); },
        kResetContentsMenuId);
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
