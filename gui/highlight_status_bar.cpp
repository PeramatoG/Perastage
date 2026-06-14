#include "highlight_status_bar.h"

// Creates a status bar with a child label for transient highlighted field text.
HighlightStatusBar::HighlightStatusBar(wxWindow *parent, wxWindowID id)
    : wxStatusBar(parent, id) {
  highlightLabel_ = new wxStaticText(this, wxID_ANY, "");
  highlightLabel_->Hide();
  Bind(wxEVT_SIZE, &HighlightStatusBar::OnSize, this);
}

// Shows highlighted text in a child label positioned over the requested field.
void HighlightStatusBar::SetHighlightedFieldText(int field, const wxString &text,
                                                 const wxColour &textColour) {
  highlightedField_.field = field;
  wxStatusBar::SetStatusText("", field);
  highlightLabel_->SetLabel(text);
  highlightLabel_->SetForegroundColour(textColour);
  highlightLabel_->SetBackgroundColour(GetBackgroundColour());
  PositionHighlightLabel();
  highlightLabel_->Show();
  highlightLabel_->Refresh();
}

// Restores normal native status-bar text and hides the highlighted child label.
void HighlightStatusBar::ClearHighlightedField(int field, const wxString &text) {
  if (highlightedField_.field == field) {
    highlightedField_.field = wxNOT_FOUND;
    highlightLabel_->Hide();
  }
  wxStatusBar::SetStatusText(text, field);
}

// Repositions highlighted child text whenever the status bar is resized.
void HighlightStatusBar::OnSize(wxSizeEvent &event) {
  PositionHighlightLabel();
  event.Skip();
}

// Aligns the highlighted label with the active status-bar field rectangle.
void HighlightStatusBar::PositionHighlightLabel() {
  if (!highlightLabel_ || highlightedField_.field == wxNOT_FOUND)
    return;

  wxRect rect;
  if (!GetFieldRect(highlightedField_.field, rect))
    return;

  constexpr int kHorizontalPadding = 4;
  rect.Deflate(kHorizontalPadding, 0);
  highlightLabel_->SetPosition(rect.GetPosition());
  highlightLabel_->SetSize(rect.GetSize());
}
