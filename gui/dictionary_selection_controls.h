#pragma once

#include <functional>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/window.h>

struct DictionarySelectionControls {
  wxStaticText *activeFileLabel = nullptr;
  wxStaticText *activePathLabel = nullptr;
  wxButton *selectButton = nullptr;
};

DictionarySelectionControls BuildDictionarySelectionControls(
    wxWindow *parent, wxSizer *parentSizer, const wxString &title,
    const wxString &buttonLabel, const std::function<void()> &onSelect);

void UpdateDictionarySelectionControls(const DictionarySelectionControls &controls,
                                       const wxString &fileName,
                                       const wxString &fullPath);
