#include "support/document_font_popup_probe.h"
#include "support/layout_text_editor_font_probe.h"
#include <cassert>
#include <set>
#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/uiaction.h>
#include <wx/utils.h>
#include <wx/sizer.h>

void TestDocumentFontPicker() {
  wxDialog host(nullptr, wxID_ANY, "Font picker test");
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  auto *picker = new gui::DocumentFontChoice(&host, {});
  sizer->Add(picker, 0, wxALL, 16);
  host.SetSizerAndFit(sizer);
  host.Centre();
  host.Show();

  std::set<std::string> names;
  bool included = false, installed = false;
  for (const auto &entry : picker->Entries()) {
    assert(names.insert(entry.family).second);
    assert(!entry.family.ends_with(" (default)") && !entry.family.ends_with(" (installed)"));
    if (entry.origin == gui::DocumentFontChoice::Origin::Installed) installed = true;
    else { assert(!installed); included = true; }
  }
  assert(included && picker->GetValue() == "PerastageSans");
  unsigned previews = 0, reversions = 0, commits = 0;
  std::string browsed;
  picker->SetPreviewHandler([&](auto candidate) {
    if (candidate) { browsed = *candidate; ++previews; }
    else { browsed.clear(); ++reversions; }
  });
  picker->SetCommitHandler([&] { ++commits; });
  picker->Popup();
  font_picker_probe::Pointer(*picker, wxEVT_MOTION, 0);
  assert(previews == 0);
  font_picker_probe::Pointer(*picker, wxEVT_MOTION, 2);
  font_picker_probe::Pointer(*picker, wxEVT_MOTION, 2);
  assert(previews == 1 && browsed == "Noto Serif" && picker->GetReference().empty());
  font_picker_probe::Press(*picker, WXK_DOWN);
  assert(browsed == "Source Sans 3" && previews == 2 && commits == 0);
  font_picker_probe::Press(*picker, WXK_ESCAPE);
  assert(!picker->IsPopupShown() && browsed.empty() && reversions == 1 && commits == 0);
  picker->Popup();
#if defined(__WXGTK__) && wxUSE_UIACTIONSIMULATOR
  wxYield();
  wxUIActionSimulator input;
  assert(input.Char(WXK_DOWN));
  for (int attempts = 0; browsed.empty() && attempts < 30; ++attempts) {
    wxMilliSleep(10);
    wxYield();
  }
  assert(browsed == "Noto Serif");
#endif
  font_picker_probe::Filter(*picker, "NOTO SERIF");
  assert(browsed == "Noto Serif" && picker->GetValue() == "PerastageSans");
#if defined(__WXGTK__) && wxUSE_UIACTIONSIMULATOR
  assert(input.MouseMove(host.GetScreenPosition() - wxPoint(10, 10)));
  assert(input.MouseClick());
  for (int attempts = 0; picker->IsPopupShown() && attempts < 30; ++attempts) {
    wxMilliSleep(10);
    wxYield();
  }
#else
  picker->Dismiss();
#endif
  assert(browsed.empty() && reversions == 2 && commits == 0);
  picker->Popup();
  font_picker_probe::Pointer(*picker, wxEVT_LEFT_DOWN, 2);
  assert(!picker->IsPopupShown() && picker->GetReference() == "Noto Serif" && commits == 1);
  picker->Popup();
  font_picker_probe::Filter(*picker, "source sans");
  font_picker_probe::Press(*picker, WXK_RETURN);
  assert(!picker->IsPopupShown() && picker->GetReference() == "Source Sans 3" && commits == 2);
  picker->SelectFamily("Missing requested family");
  assert(picker->GetReference() == "Missing requested family" && picker->GetValue() == "Missing requested family");
  picker->SetPreviewHandler({});
  picker->SetCommitHandler({});
  CheckLayoutTextEditorFontSelection(host);
}
