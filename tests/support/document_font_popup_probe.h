#pragma once

#include "document_font_choice.h"
#include <stdexcept>
#include <wx/textctrl.h>
#include <wx/vlbox.h>

namespace font_picker_probe {
inline wxWindow &PopupControl(gui::DocumentFontChoice &picker, const char *name) {
  auto *panel = picker.GetPopupControl()->GetControl();
  auto *window = panel ? wxWindow::FindWindowByName(name, panel) : nullptr;
  if (!picker.IsPopupShown() || !window)
    throw std::runtime_error("Font popup control is unavailable");
  return *window;
}

inline void Press(gui::DocumentFontChoice &picker, int code) {
  auto &search = PopupControl(picker, "documentFontSearch");
  wxKeyEvent event(wxEVT_CHAR_HOOK);
  event.m_keyCode = code;
  event.SetEventObject(&search);
  search.GetEventHandler()->ProcessEvent(event);
}

inline wxVListBox &List(gui::DocumentFontChoice &picker) {
  return dynamic_cast<wxVListBox &>(PopupControl(picker, "documentFontFamilies"));
}

inline void Pointer(gui::DocumentFontChoice &picker, wxEventType type, int row) {
  auto &list = List(picker);
  const auto rect = list.GetItemRect(row);
  if (rect.IsEmpty()) throw std::runtime_error("Font row is outside the popup");
  wxMouseEvent event(type);
  event.SetPosition(rect.GetTopLeft() + wxPoint(4, rect.GetHeight() / 2));
  event.SetEventObject(&list);
  list.GetEventHandler()->ProcessEvent(event);
}

inline void Filter(gui::DocumentFontChoice &picker, const wxString &query) {
  dynamic_cast<wxTextCtrl &>(PopupControl(picker, "documentFontSearch")).SetValue(query);
}
}
