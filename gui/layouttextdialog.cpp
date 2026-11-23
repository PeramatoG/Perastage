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
#include "layouttext_legacy_xml.h"
#include <wx/stattext.h>
#include "document_font_choice.h"
#include "typography/font_resources.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <set>

#include <wx/artprov.h>
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/richtext/richtextbuffer.h>
#include <wx/richtext/richtextxml.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include "document_text_renderer.h"

#include "layouttextutils.h"
#include "layoutviewerpanel_shared.h"
#include "projectutils.h"

namespace {
constexpr int kToolbarIconSizePx = 16;
constexpr int kToolbarButtonSizePx = 36;
constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 72;

// Native editor styling is an adapter; document rendering uses the verified
// resource bytes and shared composition regardless of native font availability.
void RegisterEditorFonts() {
#if wxUSE_PRIVATE_FONTS
  static std::set<std::string> registered;
  const auto catalog = typography::GetFontCatalog(ProjectUtils::GetResourceRoot());
  for (const auto family : catalog->Families()) {
    for (const auto [bold, italic] : {std::pair{false, false}, {true, false}, {false, true}, {true, true}}) {
      const auto resolved = catalog->Resolve(family, bold, italic);
      if (resolved.issue != typography::FontIssue::None || !resolved.face) continue;
      const auto face = resolved.face;
      if (registered.contains(face->fingerprint)) continue;
      const auto path = face->path.u8string();
      if (wxFont::AddPrivateFont(wxString::FromUTF8(reinterpret_cast<const char *>(path.data()), path.size())))
        registered.insert(face->fingerprint);
    }
  }
#endif
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
                                   bool solidBackground, bool drawFrame, wxSize documentSize,
                                   std::optional<bool> wrapText)
    : wxDialog(parent, wxID_ANY, "Edit Text", wxDefaultPosition,
               wxSize(640, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), wrapText_(wrapText) {
  EnsureRichTextHandlers();
  RegisterEditorFonts();
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
  addToolButton("italic", "Italic", [this]() { textCtrl->ApplyItalicToSelection(); });
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
  auto *family = new gui::DocumentFontChoice(this, {});
  family->SetName("layoutTextFontFamily");
  family->SetSampleRenderer([this](std::string_view family, const wxSize &size) {
    auto *content = textCtrl->GetFocusObject();
    if (!content) return wxNullBitmap;
    const auto range = textCtrl->HasSelection() ? textCtrl->GetSelectionRange().ToInternal()
                     : wxRichTextRange(0, textCtrl->GetLastPosition() - 1);
    return layouttext::RenderFontSample(*content, range, textCtrl->GetBuffer().GetBasicStyle(), family, size);
  });
  family->SetCommitHandler([this, family] {
    auto selected = family->GetFamily();
    if (selected.empty() || selected == typography::kDefaultFamily) selected = "Noto Sans";
    wxRichTextAttr attr;
    attr.SetFontFaceName(wxString::FromUTF8(selected));
    const auto range = textCtrl->HasSelection() ? textCtrl->GetSelectionRange()
                      : wxRichTextRange(0, textCtrl->GetLastPosition());
    if (range.GetLength() > 0) {
      const auto reference = family->GetReference();
      wxRichTextProperties properties;
      properties.SetProperty(layouttext::kInstalledFontReferenceProperty, wxString::FromUTF8(reference));
      textCtrl->BeginBatchUndo(_("Font family"));
      textCtrl->SetStyleEx(range, attr, wxRICHTEXT_SETSTYLE_WITH_UNDO | wxRICHTEXT_SETSTYLE_OPTIMIZE);
      const bool installed = typography::DecodeFontReference(reference).family != reference;
      const int propertyAction = installed ? 0 : wxRICHTEXT_SETPROPERTIES_REMOVE;
      textCtrl->SetProperties(range, properties, propertyAction |
          wxRICHTEXT_SETPROPERTIES_WITH_UNDO | wxRICHTEXT_SETPROPERTIES_CHARACTERS_ONLY);
      textCtrl->EndBatchUndo();
    }
    if (!textCtrl->HasSelection()) {
      auto next = textCtrl->GetDefaultStyleEx();
      next.Apply(attr);
      textCtrl->SetDefaultStyle(next);
    }
    textCtrl->SetFocus();
  });
  toolbarSizer->Add(family, 0, wxRIGHT, 8);


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
  textCtrl->SetBackgroundColour(*wxWHITE);
  textCtrl->SetForegroundColour(*wxBLACK);
  textCtrl->SetMinSize(wxSize(580, 280));
  mainSizer->Add(textCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *okButton = new wxButton(this, wxID_OK, _("Ok"));
  wxButton *cancelButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
  auto *preview = new wxButton(this, wxID_ANY, _("Document preview"));
  preview->Bind(wxEVT_BUTTON, [this, documentSize](wxCommandEvent &) { ShowDocumentPreview(documentSize); });
  buttonSizer->Add(preview, 0, wxRIGHT, 8);
  auto *refresh = new wxButton(this, wxID_ANY, _("Refresh fonts"));
  refresh->Bind(wxEVT_BUTTON, [this, family](wxCommandEvent &) {
    family->RefreshFamilies();
    RegisterEditorFonts();
  });
  buttonSizer->Add(refresh, 0, wxRIGHT, 8);
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(okButton, 0, wxRIGHT, 8);
  buttonSizer->Add(cancelButton, 0);
  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(mainSizer);
  LoadInitialContent(initialRichText, fallbackText);
  originalXml_ = initialRichText;
  legacyXmlTail_ = layouttext::LegacyRichTextTail(originalXml_, SerializeBuffer());
  if (layouttext::HasLightInk(textCtrl->GetBuffer()))
    textCtrl->SetBackgroundColour(wxColour(118, 118, 118));
  if (!legacyXmlTail_.empty()) {
    auto *notice = new wxStaticText(this, wxID_ANY, _(
        "Some legacy formatting cannot be edited here. A copy of the original is kept with your edits."));
    notice->SetName("layoutTextLegacyNotice");
    notice->Wrap(560);
    mainSizer->Insert(2, notice, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    SetMinSize(mainSizer->GetMinSize());
  }
  const auto synchronizeFamily = [this, family] {
    wxRichTextAttr attr;
    const auto pos = textCtrl->HasSelection() ? textCtrl->GetSelectionRange().GetStart()
                                             : textCtrl->GetInsertionPoint();
    if (!textCtrl->GetStyle(pos, attr)) return;
    auto name = attr.GetFontFaceName().utf8_string();
    if (const auto *object = textCtrl->GetFocusObject()->GetLeafObjectAtPosition(pos)) {
      const auto reference = object->GetProperties()
          .GetPropertyString(layouttext::kInstalledFontReferenceProperty).utf8_string();
      if (!reference.empty() && typography::DecodeFontReference(reference).family == name)
        name = reference;
    }
    family->SetFaceStyle(attr.GetFontWeight() >= wxFONTWEIGHT_BOLD,
                         attr.GetFontStyle() == wxFONTSTYLE_ITALIC || attr.GetFontStyle() == wxFONTSTYLE_SLANT);
    family->SelectFamily(name);
  };
  synchronizeFamily();
  textCtrl->Bind(wxEVT_RICHTEXT_SELECTION_CHANGED, [synchronizeFamily](wxRichTextEvent &event) {
    synchronizeFamily();
    event.Skip();
  });
  Bind(wxEVT_CLOSE_WINDOW, [family](wxCloseEvent &event) {
    family->CancelBrowsing();
    event.Skip();
  });
  Bind(wxEVT_SHOW, [family](wxShowEvent &event) {
    if (!event.IsShown()) family->CancelBrowsing();
    event.Skip();
  });
  Layout();
  // Finish the native load before recording the untouched XML baseline.
  textCtrl->Invalidate();
  textCtrl->LayoutContent();
  textCtrl->SetInsertionPoint(0);
  textCtrl->DiscardEdits();
  loadedXml_ = SerializeBuffer();
  Centre();
}

void LayoutTextDialog::ShowDocumentPreview(const wxSize &documentSize) {
  if (documentSize.x <= 0 || documentSize.y <= 0) return;
  const auto scale = std::min(650.0 / documentSize.x, 420.0 / documentSize.y);
  const wxSize output(std::max(1, static_cast<int>(documentSize.x * scale)),
                      std::max(1, static_cast<int>(documentSize.y * scale)));
  layouts::LayoutTextDefinition text;
  text.richText = GetRichText().utf8_string();
  text.wrapText = GetWrapText();
  text.text = GetPlainText().utf8_string();
  text.solidBackground = GetSolidBackground();
  const auto image = layouttext::RenderTextImage(text, output, documentSize, scale);
  if (!image.IsOk()) return;
  wxDialog preview(this, wxID_ANY, _("Document preview"));
  auto *contents = new wxBoxSizer(wxVERTICAL);
  contents->Add(new wxStaticBitmap(&preview, wxID_ANY, wxBitmap(image), wxDefaultPosition,
                                    wxDefaultSize, GetDrawFrame() ? wxBORDER_SIMPLE : wxBORDER_NONE),
                0, wxALL | wxALIGN_CENTER_HORIZONTAL, 8);
  contents->Add(preview.CreateStdDialogButtonSizer(wxOK), 0, wxALL | wxEXPAND, 8);
  preview.SetSizerAndFit(contents);
  preview.ShowModal();
}

wxString LayoutTextDialog::SerializeBuffer() const {
  wxRichTextBuffer bufferCopy = textCtrl->GetBuffer();
  return layouttext::SaveRichTextBufferToString(bufferCopy);
}

// Returns the edited content serialized as rich text XML.
wxString LayoutTextDialog::GetRichText() const {
  const auto xml = SerializeBuffer();
  if (xml == loadedXml_) return originalXml_;
  return xml.empty() ? xml : xml + legacyXmlTail_;
}

std::optional<bool> LayoutTextDialog::GetWrapText() const {
  if (wrapText_) return wrapText_;
  if (GetRichText() == originalXml_) return wrapText_;
  return true;
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

// Toggles bold formatting for the current rich text selection.
void LayoutTextDialog::ApplyBold() {
  if (textCtrl)
    textCtrl->ApplyBoldToSelection();
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
