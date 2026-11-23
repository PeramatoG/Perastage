#include "document_font_popup.h"
#include "document_font_choice.h"
#include <wx/dc.h>
#include <wx/display.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/textctrl.h>
#include <wx/vlbox.h>
#include <algorithm>
#include <utility>

namespace gui {
class FontFamilyList : public wxVListBox {
public:
  struct Row {
    wxString label;
    std::optional<size_t> entry;
  };
  explicit FontFamilyList(wxWindow *parent) : wxVListBox(parent) {
    SetName("documentFontFamilies");
  }

  std::vector<Row> rows;
  std::optional<size_t> EntryAt(int row) const {
    return row >= 0 && static_cast<size_t>(row) < rows.size() ? rows[row].entry : std::nullopt;
  }
  int RowAt(const wxPoint &point) const {
    return GetClientRect().Contains(point) ? VirtualHitTest(point.y) : wxNOT_FOUND;
  }

private:
  wxCoord OnMeasureItem(size_t) const override {
    return GetCharHeight() + FromDIP(8);
  }
  void OnDrawItem(wxDC &dc, const wxRect &rect, size_t index) const override {
    const auto &row = rows[index];
    wxFont font = GetFont();
    if (!row.entry) font.MakeBold();
    dc.SetFont(font);
    dc.SetTextForeground(IsSelected(index) ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)
        : row.entry ? GetForegroundColour() : wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    dc.DrawText(row.label, rect.x + FromDIP(row.entry ? 14 : 7),
                rect.y + (rect.height - dc.GetCharHeight()) / 2);
  }
};

bool DocumentFontPopup::Create(wxWindow *parent) {
  panel_ = new wxPanel(parent);
  auto *box = new wxBoxSizer(wxVERTICAL);
  const int pad = panel_->FromDIP(6);
  search_ = new wxTextCtrl(panel_, wxID_ANY);
  search_->SetName("documentFontSearch");
  search_->SetHint(_("Search fonts"));
  search_->SetToolTip(_("Search fonts"));
  box->Add(search_, 0, wxEXPAND | wxALL, pad);
  list_ = new FontFamilyList(panel_);
  box->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, pad);
  panel_->SetSizer(box);
  search_->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { Filter(); });
  for (auto *window : {static_cast<wxWindow *>(panel_), static_cast<wxWindow *>(search_),
                       static_cast<wxWindow *>(list_)})
    window->Bind(wxEVT_CHAR_HOOK, &DocumentFontPopup::OnKey, this);
  list_->Bind(wxEVT_MOTION, [this](wxMouseEvent &event) { Highlight(list_->RowAt(event.GetPosition())); });
  list_->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) {
    const int row = list_->RowAt(event.GetPosition());
    if (list_->EntryAt(row)) Confirm(row);
  });
  return true;
}

wxWindow *DocumentFontPopup::GetControl() { return panel_; }

wxString DocumentFontPopup::GetStringValue() const {
  return wxString::FromUTF8(choice_.GetFamily());
}

wxSize DocumentFontPopup::GetAdjustedSize(int minWidth, int, int maxHeight) {
  if (!sample_ && choice_.renderSample_) {
    sample_ = new wxStaticBitmap(panel_, wxID_ANY, wxNullBitmap);
    sample_->SetName("documentFontSample");
    sample_->SetMinSize(panel_->FromDIP(wxSize(220, 70)));
    panel_->GetSizer()->Add(sample_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, panel_->FromDIP(6));
  }
  const int screen = wxDisplay::GetFromWindow(&choice_);
  const int available = screen == wxNOT_FOUND ? panel_->FromDIP(380)
      : wxDisplay(static_cast<unsigned>(screen)).GetClientArea().width;
  const auto preferred = panel_->FromDIP(wxSize(340, sample_ ? 365 : 300));
  return {std::min(std::max(minWidth, preferred.x), available),
          std::min(maxHeight, preferred.y)};
}

void DocumentFontPopup::OnPopup() {
  active_ = true;
  search_->ChangeValue({});
  panel_->Layout();
  Filter();
  if (choice_.selected_) {
    const auto found = std::find_if(list_->rows.begin(), list_->rows.end(), [this](const auto &row) {
      return row.entry == choice_.selected_;
    });
    if (found != list_->rows.end()) Highlight(static_cast<int>(found - list_->rows.begin()));
  }
  panel_->CallAfter([this] { if (active_ && choice_.IsPopupShown()) search_->SetFocus(); });
}

void DocumentFontPopup::OnDismiss() {
  active_ = false;
  ClearPreview();
}

void DocumentFontPopup::ClearPreview() {
  highlighted_.reset();
  if (std::exchange(previewActive_, false) && choice_.preview_) choice_.preview_(std::nullopt);
}

void DocumentFontPopup::Filter() {
  if (!active_) return;
  const auto query = search_->GetValue().Lower();
  list_->rows.clear();
  std::optional<DocumentFontChoice::Origin> origin;
  for (size_t i = 0; i < choice_.entries_.size(); ++i) {
    const auto &entry = choice_.entries_[i];
    const auto label = wxString::FromUTF8(entry.family);
    if (!query.empty() && !label.Lower().Contains(query)) continue;
    if (origin != entry.origin) {
      origin = entry.origin;
      list_->rows.push_back({entry.origin == DocumentFontChoice::Origin::Included
          ? _("Included with Perastage") : _("Installed on this computer"), std::nullopt});
    }
    list_->rows.push_back({label, i});
  }
  list_->SetItemCount(list_->rows.size());
  list_->SetSelection(wxNOT_FOUND);
  list_->Refresh();
  for (size_t i = 0; i < list_->rows.size(); ++i) {
    if (list_->rows[i].entry && list_->rows[i].entry == highlighted_) {
      Highlight(static_cast<int>(i));
      return;
    }
  }
  if (!query.empty() && list_->rows.size() > 1) Highlight(1);
  else {
    ClearPreview();
    if (sample_) {
      sample_->SetBitmap(wxNullBitmap);
      sample_->SetMinSize(panel_->FromDIP(wxSize(220, 70)));
    }
  }
}

void DocumentFontPopup::Highlight(int row) {
  const auto entry = list_->EntryAt(row);
  if (!active_ || !entry) return;
  if (list_->GetSelection() != row) list_->SetSelection(row);
  if (highlighted_ == entry) return;
  highlighted_ = entry;
  const auto &name = choice_.entries_[*entry].family;
  if (choice_.preview_) {
    const bool temporary = entry != choice_.selected_;
    if (std::exchange(previewActive_, temporary) || temporary)
      choice_.preview_(temporary ? std::optional<std::string_view>(name) : std::nullopt);
  }
  if (sample_) {
    const auto size = wxSize(sample_->GetClientSize().x, panel_->FromDIP(70));
    const std::string_view reference = entry == choice_.selected_ ? choice_.requested_ : name;
    sample_->SetBitmap(choice_.renderSample_(reference, size));
    sample_->SetMinSize(panel_->FromDIP(wxSize(220, 70)));
  }
}

void DocumentFontPopup::Move(int direction) {
  int row = list_->GetSelection();
  if (row == wxNOT_FOUND && direction < 0) row = static_cast<int>(list_->rows.size());
  for (row += direction; row >= 0 && row < static_cast<int>(list_->rows.size()); row += direction) {
    if (list_->EntryAt(row)) { Highlight(row); return; }
  }
}

void DocumentFontPopup::Confirm(int row) {
  if (const auto entry = list_->EntryAt(row)) {
    const auto family = choice_.entries_[*entry].family;
    choice_.ConfirmFamily(family);
  }
}

void DocumentFontPopup::OnKey(wxKeyEvent &event) {
  switch (event.GetKeyCode()) {
  case WXK_UP: Move(-1); break;
  case WXK_DOWN: Move(1); break;
  case WXK_RETURN: case WXK_NUMPAD_ENTER: Confirm(list_->GetSelection()); break;
  case WXK_ESCAPE: choice_.CancelBrowsing(); break;
  default: event.Skip();
  }
}

void DocumentFontPopup::OnComboKeyEvent(wxKeyEvent &event) {
  if (active_) OnKey(event);
  else if (event.GetKeyCode() == WXK_DOWN || event.GetKeyCode() == WXK_SPACE)
    choice_.Popup();
  else event.Skip();
}
}
