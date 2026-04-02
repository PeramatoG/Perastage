#include "editable_focus_utils.h"

#include <wx/combobox.h>
#include <wx/grid.h>
#include <wx/spinctrl.h>
#include <wx/textentry.h>
#include <wx/window.h>

namespace gui {
namespace {

bool IsEditableInputWindow(const wxWindow *window) {
  if (!window)
    return false;

  if (const auto *textEntry = dynamic_cast<const wxTextEntry *>(window);
      textEntry && textEntry->IsEditable()) {
    return true;
  }

  if (const auto *combo = dynamic_cast<const wxComboBox *>(window);
      combo && combo->IsEditable()) {
    return true;
  }

  if (dynamic_cast<const wxSpinCtrl *>(window) ||
      dynamic_cast<const wxSpinCtrlDouble *>(window)) {
    return true;
  }

  if (const auto *grid = dynamic_cast<const wxGrid *>(window);
      grid && grid->IsCellEditControlShown()) {
    return true;
  }

  return false;
}

} // namespace

bool IsEditableWidgetFocused(const wxWindow *focusedWindow) {
  const wxWindow *current = focusedWindow;
  while (current) {
    if (IsEditableInputWindow(current))
      return true;
    current = current->GetParent();
  }
  return false;
}

} // namespace gui
