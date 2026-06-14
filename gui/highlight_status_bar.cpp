#include "highlight_status_bar.h"

#include <wx/dcbuffer.h>
#include <wx/settings.h>

// Creates a status bar that can draw one highlighted field with custom text color.
HighlightStatusBar::HighlightStatusBar(wxWindow *parent, wxWindowID id)
    : wxStatusBar(parent, id) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  Bind(wxEVT_PAINT, &HighlightStatusBar::OnPaint, this);
}

// Stores highlighted field text while keeping native field text available between paints.
void HighlightStatusBar::SetHighlightedFieldText(int field, const wxString &text,
                                                 const wxColour &textColour) {
  highlightedField_ = HighlightedField{field, text, textColour};
  wxStatusBar::SetStatusText(text, field);
  Refresh(false);
}

// Restores a field to normal native status-bar rendering.
void HighlightStatusBar::ClearHighlightedField(int field, const wxString &text) {
  if (highlightedField_ && highlightedField_->field == field)
    highlightedField_.reset();
  wxStatusBar::SetStatusText(text, field);
  Refresh(false);
}

// Paints status-bar fields with optional highlighted text for one field.
void HighlightStatusBar::OnPaint(wxPaintEvent &event) {
  (void)event;
  wxAutoBufferedPaintDC dc(this);

  wxColour background = GetBackgroundColour();
  if (!background.IsOk())
    background = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
  dc.SetFont(GetFont());

  constexpr int kHorizontalPadding = 4;
  dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));
  dc.SetBrush(wxBrush(background));
  dc.DrawRectangle(GetClientRect());

  const int fieldsCount = GetFieldsCount();
  for (int field = 0; field < fieldsCount; ++field) {
    wxRect rect;
    if (!GetFieldRect(field, rect))
      continue;

    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));
    dc.SetBrush(wxBrush(background));
    dc.DrawRectangle(rect);

    wxString text = GetStatusText(field);
    wxColour textColour = GetForegroundColour();
    if (!textColour.IsOk())
      textColour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    if (highlightedField_ && highlightedField_->field == field) {
      text = highlightedField_->text;
      textColour = highlightedField_->textColour;
    }

    dc.SetTextForeground(textColour);
    wxRect textRect = rect.Deflate(kHorizontalPadding, 0);
    dc.DrawLabel(text, textRect, wxALIGN_CENTER_VERTICAL);
  }
}
