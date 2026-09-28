#include "inspection/inspector_xml_editor.h"

#include <array>
#include <cassert>

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/stc/stc.h>

namespace {

class TestApp final : public wxApp {
public:
  // Initializes the GUI runtime required by the styled text control.
  bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(TestApp);

class AppScope final {
public:
  // Starts a minimal GUI-capable wxWidgets application.
  AppScope() {
    int argc = 0;
    char **argv = nullptr;
    started_ = wxEntryStart(argc, argv);
    if (started_ && wxTheApp)
      initialized_ = wxTheApp->CallOnInit();
  }

  // Stops the test application after all controls have been destroyed.
  ~AppScope() {
    if (started_)
      wxEntryCleanup();
  }

  // Reports whether the native GUI runtime initialized successfully.
  bool IsOk() const { return started_ && initialized_; }

private:
  bool started_ = false;
  bool initialized_ = false;
};

} // namespace

// Verifies XML lexer fold levels, markers, and contract/expand behavior.
int main() {
  AppScope app;
  if (!app.IsOk())
    return 77;
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Inspector XML test");
  auto *editor = new wxStyledTextCtrl(frame, wxID_ANY);
  gui::inspection::ConfigureInspectorXmlEditor(*editor);
  editor->SetReadOnly(false);
  editor->SetText("<root>\n  <section>\n    <child/>\n  </section>\n</root>\n");
  editor->SetReadOnly(true);
  gui::inspection::ColouriseInspectorXml(*editor);

  assert((editor->GetFoldLevel(0) & wxSTC_FOLDLEVELHEADERFLAG) != 0);
  assert((editor->GetFoldLevel(1) & wxSTC_FOLDLEVELHEADERFLAG) != 0);
  assert(editor->GetMarginMask(1) == wxSTC_MASK_FOLDERS);
  const std::array<std::pair<int, int>, 7> markers{{
      {wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS},
      {wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS},
      {wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE},
      {wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED},
      {wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED},
      {wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER},
      {wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER},
  }};
  for (const auto &[marker, symbol] : markers)
    assert(editor->GetMarkerSymbolDefined(marker) == symbol);

  editor->FoldAll(wxSTC_FOLDACTION_CONTRACT);
  assert(!editor->GetLineVisible(1));
  editor->FoldAll(wxSTC_FOLDACTION_EXPAND);
  assert(editor->GetLineVisible(1));
  editor->FoldAll(wxSTC_FOLDACTION_CONTRACT);
  editor->FoldAll(wxSTC_FOLDACTION_CONTRACT);
  editor->FoldAll(wxSTC_FOLDACTION_EXPAND);
  assert(editor->GetLineVisible(2));
  frame->Destroy();
  wxTheApp->ProcessPendingEvents();
  return 0;
}
