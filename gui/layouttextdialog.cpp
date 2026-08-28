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
#include <wx/richtext/richtextbuffer.h>
#include <wx/richtext/richtextxml.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>

#include "layouttextutils.h"
#include "layoutviewerpanel_shared.h"
#include "projectutils.h"

namespace {
constexpr int kToolbarIconSizePx = 16;
constexpr int kToolbarButtonSizePx = 36;
constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 72;

// Returns the text colour used while editing rich text.
wxColour GetEditorTextColour() {
  return *wxWHITE;
}

// Applies a consistent text colour to the whole rich text buffer.
void NormalizeBufferTextColour(wxRichTextBuffer &buffer,
                               const wxColour &colour) {
  wxRichTextRange range = buffer.GetRange();
  if (range.GetLength() <= 0)
    return;
  wxRichTextAttr attr;
  attr.SetTextColour(colour);
  attr.SetFlags(wxTEXT_ATTR_TEXT_COLOUR);
  buffer.SetStyle(range, attr);
}

// Initializes rich text handlers once before loading or saving rich text content.
void EnsureRichTextHandlers() {
  static bool initialized = false;
  if (initialized)
    return;
  wxRichTextBuffer::InitStandardHandlers();
  if (!wxRichTextBuffer::FindHandler(wxRICHTEXT_TYPE_XML)) {
    wxRichTextBuffer::AddHandler(new wxRichTextXMLHandler);
  }
  initialized = true;
}
} // namespace

// Creates the rich text editing dialog and wires its toolbar controls.
LayoutTextDialog::LayoutTextDialog(wxWindow *parent,
                                   const wxString &initialRichText,
                                   const wxString &fallbackText,
                                   bool solidBackground, bool drawFrame)
    : wxDialog(parent, wxID_ANY, "Edit Text", wxDefaultPosition,
               wxSize(640, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  EnsureRichTextHandlers();
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
        wxSize(kToolbarButtonSizePx, kToolbarButtonSizePx));
    button->SetMinSize(wxSize(kToolbarButtonSizePx, kToolbarButtonSizePx));
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
                                layoutviewerpanel::detail::kTextDefaultFontSize);
  fontSizeCtrl->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) {
    ApplyFontSize(fontSizeCtrl->GetValue());
  });
  fontSizeCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent &) {
    ApplyFontSize(fontSizeCtrl->GetValue());
  });
  toolbarSizer->Add(fontSizeCtrl, 0, wxRIGHT, 8);

  addToolButton("text-align-start", "Align start",
                [this]() { ApplyAlignment(wxTEXT_ALIGNMENT_LEFT); });
  addToolButton("text-align-center", "Align center",
                [this]() { ApplyAlignment(wxTEXT_ALIGNMENT_CENTRE); });
  addToolButton("text-align-end", "Align end",
                [this]() { ApplyAlignment(wxTEXT_ALIGNMENT_RIGHT); });
  addToolButton("text-align-center", "Justify",
                [this]() { ApplyAlignment(wxTEXT_ALIGNMENT_JUSTIFIED); });

  mainSizer->Add(toolbarSizer, 0, wxEXPAND | wxALL, 8);

  wxBoxSizer *optionsSizer = new wxBoxSizer(wxHORIZONTAL);
  solidBackgroundCtrl = new wxCheckBox(this, wxID_ANY, _("Solid background"));
  solidBackgroundCtrl->SetValue(solidBackground);
  optionsSizer->Add(solidBackgroundCtrl, 0, wxRIGHT, 12);
  drawFrameCtrl = new wxCheckBox(this, wxID_ANY, _("Show outline"));
  drawFrameCtrl->SetValue(drawFrame);
  optionsSizer->Add(drawFrameCtrl, 0, wxRIGHT, 8);
  mainSizer->Add(optionsSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

  textCtrl = new wxRichTextCtrl(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_RICH2);
  ApplyDefaultFontStyle();
  textCtrl->SetMinSize(wxSize(580, 280));
  mainSizer->Add(textCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *okButton = new wxButton(this, wxID_OK, _("Ok"));
  wxButton *cancelButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(okButton, 0, wxRIGHT, 8);
  buttonSizer->Add(cancelButton, 0);
  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(mainSizer);
  LoadInitialContent(initialRichText, fallbackText);
  ApplyDefaultFontStyle();
  Layout();
  Centre();
}

// Returns the edited content serialized as rich text XML.
wxString LayoutTextDialog::GetRichText() const {
  if (!textCtrl)
    return wxEmptyString;
  wxRichTextBuffer bufferCopy = textCtrl->GetBuffer();
  NormalizeBufferTextColour(bufferCopy, *wxBLACK);
  return layouttext::SaveRichTextBufferToString(bufferCopy);
}

// Returns the edited content as plain text.
wxString LayoutTextDialog::GetPlainText() const {
  return textCtrl ? textCtrl->GetBuffer().GetText() : wxString();
}

// Returns whether the text should use a solid background.
bool LayoutTextDialog::GetSolidBackground() const {
  return solidBackgroundCtrl ? solidBackgroundCtrl->GetValue() : true;
}

// Returns whether the text frame should be drawn.
bool LayoutTextDialog::GetDrawFrame() const {
  return drawFrameCtrl ? drawFrameCtrl->GetValue() : true;
}

// Loads a toolbar icon from project resources with an art-provider fallback.
wxBitmapBundle LayoutTextDialog::LoadIcon(const std::string &name) const {
  auto svgPath = ProjectUtils::ResolveResourcePath(
      std::filesystem::path("icons") / "outline" / (name + ".svg"));
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

// Loads rich text content when possible and falls back to plain text.
void LayoutTextDialog::LoadInitialContent(const wxString &initialRichText,
                                          const wxString &fallbackText) {
  if (!textCtrl)
    return;
  if (!initialRichText.empty()) {
    if (layouttext::LoadRichTextBufferFromString(textCtrl->GetBuffer(),
                                                 initialRichText)) {
      return;
    }
  }
  textCtrl->SetValue(fallbackText);
}

// Applies the default editor text style to current and future content.
void LayoutTextDialog::ApplyDefaultFontStyle() {
  if (!textCtrl)
    return;
  wxRichTextAttr defaultStyle = textCtrl->GetDefaultStyle();
  wxColour editorColour = GetEditorTextColour();
  defaultStyle.SetTextColour(editorColour);
  defaultStyle.SetFlags(defaultStyle.GetFlags() | wxTEXT_ATTR_TEXT_COLOUR);
  textCtrl->SetDefaultStyle(defaultStyle);
  textCtrl->GetBuffer().SetDefaultStyle(defaultStyle);
  textCtrl->GetBuffer().SetBasicStyle(defaultStyle);

  wxRichTextRange range(0, textCtrl->GetLastPosition());
  if (range.GetLength() <= 0)
    return;
  wxRichTextAttr colourOnly;
  colourOnly.SetTextColour(editorColour);
  colourOnly.SetFlags(wxTEXT_ATTR_TEXT_COLOUR);
  textCtrl->SetStyle(range, colourOnly);
}

// Toggles bold formatting for the current rich text selection.
void LayoutTextDialog::ApplyBold() {
  if (textCtrl)
    textCtrl->ApplyBoldToSelection();
}

// Toggles italic formatting for the current rich text selection.
void LayoutTextDialog::ApplyItalic() {
  if (textCtrl)
    textCtrl->ApplyItalicToSelection();
}

// Applies the selected font size to the current selection or document.
void LayoutTextDialog::ApplyFontSize(int size) {
  if (!textCtrl)
    return;
  wxRichTextAttr attr;
  attr.SetFontSize(size);
  attr.SetFlags(wxTEXT_ATTR_FONT_SIZE);
  wxRichTextRange range = textCtrl->GetSelectionRange();
  if (textCtrl->HasSelection() && range.GetLength() > 0) {
    textCtrl->SetStyleEx(range, attr);
  } else {
    wxRichTextRange all(0, textCtrl->GetLastPosition());
    if (all.GetLength() > 0)
      textCtrl->SetStyleEx(all, attr);
    wxRichTextAttr defaultStyle = textCtrl->GetDefaultStyle();
    defaultStyle.SetFontSize(size);
    defaultStyle.SetFlags(defaultStyle.GetFlags() | wxTEXT_ATTR_FONT_SIZE);
    textCtrl->SetDefaultStyle(defaultStyle);
  }
}

// Adjusts the font size control by the requested delta and applies it.
void LayoutTextDialog::AdjustFontSize(int delta) {
  if (!fontSizeCtrl)
    return;
  int size = fontSizeCtrl->GetValue() + delta;
  size = std::clamp(size, kMinFontSize, kMaxFontSize);
  fontSizeCtrl->SetValue(size);
  ApplyFontSize(size);
}

// Applies paragraph alignment to the current rich text selection.
void LayoutTextDialog::ApplyAlignment(int alignment) {
  if (!textCtrl)
    return;
  textCtrl->ApplyAlignmentToSelection(
      static_cast<wxTextAttrAlignment>(alignment));
}
