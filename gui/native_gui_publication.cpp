#include "native_gui_publication.h"

#include <wx/glcanvas.h>
#include <wx/dataview.h>
#include <wx/settings.h>
#include <wx/textctrl.h>
#include <wx/window.h>

namespace {
bool ContainsGlCanvas(wxWindow &window) {
  if (dynamic_cast<wxGLCanvas *>(&window))
    return true;
  for (wxWindow *child : window.GetChildren()) {
    if (!child->IsTopLevel() && ContainsGlCanvas(*child))
      return true;
  }
  return false;
}

void InitializeNativeSubtree(wxWindow &window) {
  if (!window.UseBackgroundColour()) {
    const bool contentControl = dynamic_cast<wxDataViewCtrl *>(&window) ||
                                dynamic_cast<wxTextCtrl *>(&window);
    window.SetOwnBackgroundColour(wxSystemSettings::GetColour(
        contentControl ? wxSYS_COLOUR_WINDOW : wxSYS_COLOUR_BTNFACE));
  }
  for (wxWindow *child : window.GetChildren()) {
    if (!child->IsTopLevel())
      InitializeNativeSubtree(*child);
  }
}

void UpdateNativeSubtree(wxWindow &window) {
  window.Update();
  // MSW Update() paints only this HWND. Native DataView internals are painted
  // by their control; wx child windows still need their own Update().
  for (wxWindow *child : window.GetChildren()) {
    if (!child->IsTopLevel() && child->IsShownOnScreen())
      UpdateNativeSubtree(*child);
  }
}
}

void gui::InitializeNativeGuiBackgrounds(wxWindow &root) {
  for (wxWindow *child : root.GetChildren()) {
    if (child->IsTopLevel() || dynamic_cast<wxGLCanvas *>(child))
      continue;
    if (ContainsGlCanvas(*child))
      InitializeNativeGuiBackgrounds(*child);
    else
      InitializeNativeSubtree(*child);
  }
}

void gui::PublishNativeGuiChildren(wxWindow &root) {
  for (wxWindow *child : root.GetChildren()) {
    if (child->IsTopLevel() || !child->IsShownOnScreen() ||
        dynamic_cast<wxGLCanvas *>(child))
      continue;
    if (ContainsGlCanvas(*child)) {
      PublishNativeGuiChildren(*child);
    } else {
      // Refresh/Update can recurse into native children on GTK and MSW. Never
      // invoke them on a subtree containing a GL viewport or on the frame.
      child->Refresh(false);
      UpdateNativeSubtree(*child);
    }
  }
}
