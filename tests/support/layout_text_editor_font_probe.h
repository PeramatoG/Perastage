#pragma once

#include "layouttextdialog.h"
#include "layouttextutils.h"
#include "typography/font_reference.h"
#include <cstdlib>
#include <stdexcept>
#include <wx/choice.h>
#include <wx/richtext/richtextctrl.h>

inline void CheckLayoutTextEditorFontSelection(wxWindow &parent) {
  LayoutTextDialog dialog(&parent, {}, "Title Notes", true, true);
  dialog.Show();
  auto *family = dynamic_cast<wxChoice *>(
      dialog.FindWindowByName("layoutTextFontFamily", &dialog));
  wxRichTextCtrl *rich = nullptr;
  for (auto *child : dialog.GetChildren()) {
    if (auto *control = dynamic_cast<wxRichTextCtrl *>(child)) rich = control;
  }
  if (!family || !rich || family->FindString("Noto Serif") == wxNOT_FOUND ||
      family->FindString("Source Sans 3") == wxNOT_FOUND || family->FindString("Noto Sans Mono") == wxNOT_FOUND)
    throw std::runtime_error("The native Layout editor is missing bundled families");

  const auto select = [&](int start, int stop, int index) {
    rich->SetSelection(start, stop);
    family->SetSelection(index);
    wxCommandEvent change(wxEVT_CHOICE, family->GetId());
    change.SetEventObject(family);
    family->GetEventHandler()->ProcessEvent(change);
  };
  select(0, 5, family->FindString("Source Sans 3"));
  select(6, 11, family->FindString("Noto Serif"));

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
    const auto index = family->FindString(wxString::FromUTF8(std::string(font) + " (installed)"));
    if (index == wxNOT_FOUND) throw std::runtime_error("The Layout editor is missing the installed font");
    const auto before = dialog.GetRichText();
    select(6, 11, index);
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
  if (!opened.FindWindowByName("layoutTextLegacyNotice", &opened))
    throw std::runtime_error("Legacy content loss was not disclosed");
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
