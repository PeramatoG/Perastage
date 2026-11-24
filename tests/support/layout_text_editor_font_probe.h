#pragma once

#include "layouttextdialog.h"
#include "document_font_popup_probe.h"
#include "layouttextutils.h"
#include "typography/font_reference.h"
#include <cstdlib>
#include <stdexcept>
#include <wx/statbmp.h>
#include <algorithm>
#include <wx/richtext/richtextctrl.h>

inline void CheckLayoutTextEditorFontSelection(wxWindow &parent) {
  LayoutTextDialog dialog(&parent, {}, "Title Notes", true, true);
  dialog.Show();
  auto *family = dynamic_cast<gui::DocumentFontChoice *>(
      dialog.FindWindowByName("layoutTextFontFamily", &dialog));
  wxRichTextCtrl *rich = nullptr;
  for (auto *child : dialog.GetChildren()) {
    if (auto *control = dynamic_cast<wxRichTextCtrl *>(child)) rich = control;
  }
  if (!family || !rich || !std::any_of(family->Entries().begin(), family->Entries().end(),
      [](const auto &entry) { return entry.family == "Noto Serif"; }))
    throw std::runtime_error("The native Layout editor is missing bundled families");

  rich->SetSelection(0, 5);
  const auto beforeBrowse = dialog.GetRichText();
  const bool hadUndo = rich->CanUndo();
  family->Popup();
  font_picker_probe::Filter(*family, "noto serif");
  auto &sample = dynamic_cast<wxStaticBitmap &>(
      font_picker_probe::PopupControl(*family, "documentFontSample"));
  const auto serif = sample.GetBitmap().ConvertToImage();
  font_picker_probe::Filter(*family, "source sans 3");
  const auto sans = sample.GetBitmap().ConvertToImage();
  if (!serif.IsOk() || !sans.IsOk() || serif.GetSize() != sans.GetSize() ||
      std::equal(serif.GetData(), serif.GetData() + serif.GetWidth() * serif.GetHeight() * 3, sans.GetData()))
    throw std::runtime_error("The document sample did not preview the selected face");
  if (dialog.GetRichText() != beforeBrowse || rich->CanUndo() != hadUndo)
    throw std::runtime_error("Browsing changed the rich-text buffer or undo history");
  font_picker_probe::Press(*family, WXK_ESCAPE);
  if (family->IsPopupShown() || rich->GetSelectionRange() != wxRichTextRange(0, 5))
    throw std::runtime_error("Canceling font browsing changed the text selection");

  const auto select = [&](int start, int stop, std::string_view name) {
    rich->SetSelection(start, stop);
    family->Popup();
    font_picker_probe::Filter(*family, wxString::FromUTF8(name.data(), name.size()));
    font_picker_probe::Press(*family, WXK_RETURN);
    if (family->IsPopupShown()) throw std::runtime_error("Enter did not confirm the font");
  };
  select(0, 5, "Source Sans 3");
  select(6, 11, "Noto Serif");

  layouts::LayoutTextDefinition content;
  content.richText = dialog.GetRichText().utf8_string();
  const auto extracted = layouttext::BuildLayoutTextExportData(content);
  bool title = false, notes = false;
  for (const auto &line : extracted.lines) {
    for (const auto &run : line.runs) {
      title |= run.text == "Title" && run.family == "Source Sans 3";
      notes |= run.text == "Notes" && run.family == "Noto Serif";
    }
  }
  if (!title || !notes)
    throw std::runtime_error("The Layout editor lost a selected span's font family");
  const auto preview = layouttext::RenderTextImage(content, wxSize(720, 360), wxSize(480, 240), 1.5);
  if (!preview.IsOk() || preview.GetWidth() != 720 || preview.GetHeight() != 360)
    throw std::runtime_error("Shared document preview failed to render the edited spans");

  if (const auto *font = std::getenv("PERASTAGE_SMOKE_INSTALLED_FONT"); font && *font) {
    const auto before = dialog.GetRichText();
    select(6, 11, font);
    content.richText = dialog.GetRichText().utf8_string();
    content.wrapText = dialog.GetWrapText();
    const auto live = layouttext::BuildLayoutTextExportData(content);
    bool hasIdentity = false;
    for (const auto &run : live.lines.front().runs) {
      const auto identity = typography::DecodeFontReference(run.family);
      hasIdentity |= run.text == "Notes" && identity.family == font && !identity.fingerprints[0].empty();
    }
    if (!hasIdentity || !live.wrap) throw std::runtime_error("The styled run lost the installed face identity");
    rich->Undo();
    if (dialog.GetRichText() != before) throw std::runtime_error("Undo did not restore the full font change");
    rich->Redo();
    if (dialog.GetRichText().utf8_string() != content.richText)
      throw std::runtime_error("Redo changed the installed font identity");
    rich->Undo();
  }

  auto originalXml = dialog.GetRichText();
  originalXml.Replace("<richtext", "<!-- Legacy vendor metadata -->\n<richtext");
  LayoutTextDialog opened(&parent, originalXml, {}, true, true);
  if (opened.GetRichText() != originalXml)
    throw std::runtime_error("Opening rich text rewrote legacy XML");
  if (opened.FindWindowByName("layoutTextLegacyNotice", &opened))
    throw std::runtime_error("The retired legacy notice is still displayed");
  for (auto *child : opened.GetChildren()) {
    if (auto *control = dynamic_cast<wxRichTextCtrl *>(child)) {
      control->SetInsertionPointEnd();
      control->WriteText("!");
      const auto edited = opened.GetRichText();
      if (!edited.Contains("Perastage legacy rich text") || !opened.GetPlainText().EndsWith("!"))
        throw std::runtime_error("The rich-text edit lost the source recovery copy");
      control->Undo();
      if (opened.GetRichText() != originalXml)
        throw std::runtime_error("Undo did not restore the exact legacy XML");
      control->Redo();
      if (opened.GetRichText() != edited)
        throw std::runtime_error("Redo changed the legacy recovery copy");
    }
  }
  dialog.Hide();
}
