// This file is part of Perastage. License: GPL-3.0-or-later.
#include "properties_panel.h"
#include "document_font_choice.h"

#include <cmath>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clrpicker.h>
#include <wx/intl.h>
#include <wx/numformatter.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/textctrl.h>

namespace gui::properties {
Panel::Panel(wxWindow *parent, Read read, Commit commit)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxTAB_TRAVERSAL),
      read_(std::move(read)), commit_(std::move(commit)) {
  SetScrollRate(0, FromDIP(12));
  auto *outer = new wxBoxSizer(wxVERTICAL);
  status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
  outer->Add(status_, 0, wxEXPAND | wxALL, FromDIP(8));
  status_->Hide();
  SetSizer(outer);
}

void Panel::RequestRefresh() {
  if (pending_ || committing_) return;
  pending_ = true;
  CallAfter([this] {
    pending_ = false;
    if (!committing_) Rebuild();
  });
}

void Panel::Rebuild() {
  const auto next = read_();
  if (body_ && next == sheet_ && !rejected_) {
    if (status_->IsShown()) {
      status_->Hide();
      Layout();
      FitInside();
    }
    return;
  }
  int scrollX = 0, scrollY = 0;
  wxString focusedName;
  if (body_ && sheet_.title == next.title) {
    GetViewStart(&scrollX, &scrollY);
    if (auto *focused = wxWindow::FindFocus(); focused && body_->IsDescendant(focused))
      focusedName = focused->GetName();
  }
  sheet_ = next;
  Freeze();
  if (body_) {
    GetSizer()->Detach(body_);
    body_->Destroy();
  }
  body_ = new wxPanel(this);
  auto *content = new wxBoxSizer(wxVERTICAL);
  auto *title = new wxStaticText(body_, wxID_ANY, sheet_.title,
                                wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
  title->SetFont(title->GetFont().Bold());
  content->Add(title, 0, wxEXPAND | wxALL, FromDIP(8));
  for (const auto &section : sheet_.sections) {
    auto *box = new wxStaticBoxSizer(wxVERTICAL, body_, section.label);
    for (const auto &row : section.rows) AddRow(*box, row);
    content->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
  }
  body_->SetSizer(content);
  GetSizer()->Insert(0, body_, 0, wxEXPAND);
  status_->SetLabel(rejected_ ? _("The property could not be changed.") : wxString{});
  status_->Show(rejected_);
  rejected_ = false;
  Layout();
  FitInside();
  Scroll(scrollX, scrollY);
  if (!focusedName.empty())
    if (auto *control = wxWindow::FindWindowByName(focusedName, body_)) control->SetFocus();
  Thaw();
}

void Panel::Submit(int id, const Value &value) {
  if (committing_) return;
  committing_ = true;
  rejected_ = !commit_(id, value);
  committing_ = false;
  RequestRefresh();
}

void Panel::AddRow(wxSizer &section, const Row &row) {
  const auto submit = [this, id = row.id, generation = sheet_.generation](const Value &value) {
    if (generation == sheet_.generation) Submit(id, value);
  };
  auto *parent = static_cast<wxStaticBoxSizer &>(section).GetStaticBox();
  const auto add = [&](wxWindow *window) {
    section.Add(window, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(6));
  };
  if (std::holds_alternative<Action>(row.value)) {
    auto *button = new wxButton(parent, wxID_ANY, row.label);
    button->Bind(wxEVT_BUTTON, [submit](wxCommandEvent &) { submit(Action{}); });
    add(button);
    return;
  }
  if (const auto *value = std::get_if<bool>(&row.value)) {
    auto *checkbox = new wxCheckBox(parent, wxID_ANY, row.label);
    checkbox->SetValue(*value);
    checkbox->Enable(row.editable);
    checkbox->Bind(wxEVT_CHECKBOX, [submit](wxCommandEvent &e) { submit(e.IsChecked()); });
    add(checkbox);
    return;
  }
  add(new wxStaticText(parent, wxID_ANY, row.label));
  if (const auto *choice = std::get_if<Choice>(&row.value)) {
    auto *control = new wxChoice(parent, wxID_ANY);
    for (const auto &label : choice->options) control->Append(label);
    control->SetSelection(choice->selected);
    control->Enable(row.editable);
    control->Bind(wxEVT_CHOICE, [control, submit](wxCommandEvent &) {
      Choice value{{}, control->GetSelection()};
      submit(value);
    });
    add(control);
    return;
  }
  if (const auto *font = std::get_if<Font>(&row.value); font && row.editable) {
    auto *control = new DocumentFontChoice(parent, font->reference);
    control->SetFaceStyle(font->bold, font->italic);
    control->SetCommitHandler([control, submit, bold = font->bold, italic = font->italic] {
      submit(Font{control->GetReference(), bold, italic});
    });
    add(control);
    return;
  }
  if (const auto *color = std::get_if<wxColour>(&row.value)) {
    auto *control = new wxColourPickerCtrl(parent, wxID_ANY, *color);
    control->Enable(row.editable);
    control->Bind(wxEVT_COLOURPICKER_CHANGED,
                  [submit](wxColourPickerEvent &e) { submit(e.GetColour()); });
    add(control);
    return;
  }

  const bool numeric = std::holds_alternative<double>(row.value);
  const wxString initial = numeric
      ? wxNumberFormatter::ToString(std::get<double>(row.value), 3,
                                    wxNumberFormatter::Style_NoTrailingZeroes)
      : std::holds_alternative<Font>(row.value)
          ? wxString::FromUTF8(std::get<Font>(row.value).reference)
          : std::get<wxString>(row.value);
  const long style = row.editable ? wxTE_PROCESS_ENTER : wxTE_READONLY;
  auto *control = new wxTextCtrl(parent, wxID_ANY, initial, wxDefaultPosition,
      row.multiline ? FromDIP(wxSize(140, 72)) : wxDefaultSize,
      style | (row.multiline ? wxTE_MULTILINE : 0));
  control->SetName(row.label);
  control->SetMinSize(FromDIP(wxSize(110, -1)));
  if (row.editable) {
    control->SetToolTip(_("Press Enter to apply or Esc to cancel."));
    control->Bind(wxEVT_TEXT_ENTER, [control, initial, numeric, submit](wxCommandEvent &) {
      const auto value = control->GetValue();
      if (value == initial) return;
      double number;
      if (numeric && wxNumberFormatter::FromString(value, &number) && std::isfinite(number))
        submit(number);
      else
        submit(value);
    });
    control->Bind(wxEVT_CHAR_HOOK, [control, initial](wxKeyEvent &e) {
      if (e.GetKeyCode() == WXK_ESCAPE) control->ChangeValue(initial);
      else e.Skip();
    });
  }
  add(control);
}
} // namespace gui::properties
