#pragma once

#include <wx/colour.h>
#include <wx/stattext.h>
#include <wx/statusbr.h>

class HighlightStatusBar : public wxStatusBar {
public:
  explicit HighlightStatusBar(wxWindow *parent, wxWindowID id = wxID_ANY);

  void SetHighlightedFieldText(int field, const wxString &text,
                               const wxColour &textColour);
  void ClearHighlightedField(int field, const wxString &text);

private:
  struct HighlightedField {
    int field = wxNOT_FOUND;
  };

  void OnSize(wxSizeEvent &event);
  void PositionHighlightLabel();

  HighlightedField highlightedField_;
  wxStaticText *highlightLabel_ = nullptr;
};
