#pragma once

#include "layouttextdialog.h"
#include "layouttextutils.h"
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
  if (!family || !rich || family->GetCount() != 4)
    throw std::runtime_error("The native Layout editor is missing approved families");

  const auto select = [&](int start, int stop, int index) {
    rich->SetSelection(start, stop);
    family->SetSelection(index);
    wxCommandEvent change(wxEVT_CHOICE, family->GetId());
    change.SetEventObject(family);
    family->GetEventHandler()->ProcessEvent(change);
  };
  select(0, 5, 2);
  select(6, 11, 1);

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
  dialog.Hide();
}
