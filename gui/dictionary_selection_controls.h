#pragma once

#include <functional>

#include <wx/button.h>
#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/window.h>

struct DictionarySelectionControls {
  wxStaticText *activeFileLabel = nullptr;
  wxStaticText *activePathLabel = nullptr;
  wxButton *openButton = nullptr;
  wxButton *newButton = nullptr;
  wxButton *moreButton = nullptr;
  wxMenu *moreMenu = nullptr;
};

DictionarySelectionControls BuildDictionarySelectionControls(
    wxWindow *parent, wxSizer *parentSizer, const wxString &title,
    const std::function<void()> &onOpen, const std::function<void()> &onNew,
    const std::function<void()> &onDuplicate,
    const std::function<void()> &onUseDefault,
    const std::function<void()> &onReset);

void UpdateDictionarySelectionControls(
    const DictionarySelectionControls &controls, const wxString &fileName,
    const wxString &fullPath);
