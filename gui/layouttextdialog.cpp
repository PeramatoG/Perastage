/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "layouttextdialog.h"

#include <algorithm>
#include <filesystem>
#include <functional>

#include <wx/artprov.h>
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/sstream.h>

#include "projectutils.h"

namespace {
constexpr int kToolbarIconSizePx = 16;
constexpr int kDefaultFontSize = 12;
constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 72;
} // namespace

LayoutTextDialog::LayoutTextDialog(wxWindow *parent,
                                   const wxString &initialRichText,
                                   const wxString &fallbackText,
                                   bool solidBackground, bool drawFrame)
    : wxDialog(parent, wxID_ANY, "Edit Text", wxDefaultPosition,
               wxSize(640, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *toolbarSizer = new wxBoxSizer(wxHORIZONTAL);

  auto addToolButton = [&](const std::string &iconName,
                           const wxString &tooltip,
                           const std::function<void()> &handler) {
    wxBitmapBundle bundle = LoadIcon(iconName);
    wxBitmap bitmap = bundle.GetBitmap(wxSize(kToolbarIconSizePx,
                                              kToolbarIconSizePx));
    wxBitmapButton *button = new wxBitmapButton(
        this, wxID_ANY, bitmap, wxDefaultPosition,
        wxSize(kToolbarIconSizePx + 6, kToolbarIconSizePx + 6));
    button->SetToolTip(tooltip);
    button->Bind(wxEVT_BUTTON, [handler](wxCommandEvent &) { handler(); });
    toolbarSizer->Add(button, 0, wxRIGHT, 4);
  };

  addToolButton("bold", "Bold", [this]() { ApplyBold(); });
  addToolButton("italic", "Italic", [this]() { ApplyItalic(); });
  addToolButton("a-arrow-down", "Decrease font size",
                [this]() { AdjustFontSize(-1); });
  addToolButton("a-arrow-up", "Increase font size",
                [this]() { AdjustFontSize(1); });

  fontSizeCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxSize(64, -1),
                                wxSP_ARROW_KEYS, kMinFontSize, kMaxFontSize,
                                kDefaultFontSize);
  fontSizeCtrl->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) {
    ApplyFontSize(fontSizeCtrl->GetValue());
  });
  fontSizeCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent &) {
    ApplyFontSize(fontSizeCtrl->GetValue());
  });
  toolbarSizer->Add(fontSizeCtrl, 0, wxRIGHT, 8);

  addToolButton("align-horizontal-justify-start", "Align start",
                [this]() { ApplyAlignment(wxTEXT_ALIGNMENT_LEFT); });
  addToolButton("align-horizontal-justify-center", "Align center",
                [this]() { ApplyAlignment(wxTEXT_ALIGNMENT_CENTRE); });
  addToolButton("align-horizontal-justify-end", "Align end",
                [this]() { ApplyAlignment(wxTEXT_ALIGNMENT_RIGHT); });
  addToolButton("align-horizontal-space-between", "Justify",
                [this]() { ApplyAlignment(wxTEXT_ALIGNMENT_JUSTIFIED); });

  mainSizer->Add(toolbarSizer, 0, wxEXPAND | wxALL, 8);

  wxBoxSizer *optionsSizer = new wxBoxSizer(wxHORIZONTAL);
  solidBackgroundCtrl = new wxCheckBox(this, wxID_ANY, "Solid background");
  solidBackgroundCtrl->SetValue(solidBackground);
  optionsSizer->Add(solidBackgroundCtrl, 0, wxRIGHT, 12);
  drawFrameCtrl = new wxCheckBox(this, wxID_ANY, "Show outline");
  drawFrameCtrl->SetValue(drawFrame);
  optionsSizer->Add(drawFrameCtrl, 0, wxRIGHT, 8);
  mainSizer->Add(optionsSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

  textCtrl = new wxRichTextCtrl(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_RICH2);
  textCtrl->SetMinSize(wxSize(580, 280));
  mainSizer->Add(textCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *okButton = new wxButton(this, wxID_OK, "Ok");
  wxButton *cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(okButton, 0, wxRIGHT, 8);
  buttonSizer->Add(cancelButton, 0);
  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(mainSizer);
  LoadInitialContent(initialRichText, fallbackText);
  Layout();
  Centre();
}

wxString LayoutTextDialog::GetRichText() const {
  if (!textCtrl)
    return wxEmptyString;
  wxStringOutputStream output;
  if (textCtrl->GetBuffer().SaveFile(output, wxRICHTEXT_TYPE_XML))
    return output.GetString();
  return textCtrl->GetValue();
}

wxString LayoutTextDialog::GetPlainText() const {
  return textCtrl ? textCtrl->GetValue() : wxString();
}

bool LayoutTextDialog::GetSolidBackground() const {
  return solidBackgroundCtrl ? solidBackgroundCtrl->GetValue() : true;
}

bool LayoutTextDialog::GetDrawFrame() const {
  return drawFrameCtrl ? drawFrameCtrl->GetValue() : true;
}

wxBitmapBundle LayoutTextDialog::LoadIcon(const std::string &name) const {
  auto svgPath = ProjectUtils::GetResourceRoot() / "icons" / "outline" /
                 (name + ".svg");
  if (std::filesystem::exists(svgPath)) {
    wxBitmapBundle bundle =
        wxBitmapBundle::FromSVGFile(svgPath.string(),
                                    wxSize(kToolbarIconSizePx,
                                           kToolbarIconSizePx));
    if (bundle.IsOk())
      return bundle;
  }
  return wxArtProvider::GetBitmapBundle(wxART_MISSING_IMAGE, wxART_TOOLBAR,
                                        wxSize(kToolbarIconSizePx,
                                               kToolbarIconSizePx));
}

void LayoutTextDialog::LoadInitialContent(const wxString &initialRichText,
                                          const wxString &fallbackText) {
  if (!textCtrl)
    return;
  if (!initialRichText.empty()) {
    wxStringInputStream input(initialRichText);
    if (textCtrl->GetBuffer().LoadFile(input, wxRICHTEXT_TYPE_XML)) {
      return;
    }
  }
  textCtrl->SetValue(fallbackText);
}

void LayoutTextDialog::ApplyBold() {
  if (textCtrl)
    textCtrl->ApplyBoldToSelection();
}

void LayoutTextDialog::ApplyItalic() {
  if (textCtrl)
    textCtrl->ApplyItalicToSelection();
}

void LayoutTextDialog::ApplyFontSize(int size) {
  if (!textCtrl)
    return;
  wxRichTextAttr attr;
  attr.SetFontSize(size);
  wxRichTextRange range = textCtrl->GetSelectionRange();
  if (textCtrl->HasSelection() && range.GetLength() > 0) {
    textCtrl->SetStyleEx(range, attr);
  } else {
    wxRichTextRange all(0, textCtrl->GetLastPosition());
    if (all.GetLength() > 0)
      textCtrl->SetStyleEx(all, attr);
    textCtrl->SetDefaultStyle(attr);
  }
}

void LayoutTextDialog::AdjustFontSize(int delta) {
  if (!fontSizeCtrl)
    return;
  int size = fontSizeCtrl->GetValue() + delta;
  size = std::clamp(size, kMinFontSize, kMaxFontSize);
  fontSizeCtrl->SetValue(size);
  ApplyFontSize(size);
}

void LayoutTextDialog::ApplyAlignment(int alignment) {
  if (!textCtrl)
    return;
  textCtrl->ApplyAlignmentToSelection(
      static_cast<wxTextAttrAlignment>(alignment));
}
