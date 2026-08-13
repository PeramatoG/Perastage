#include "editable_focus_utils.h"

#include <wx/app.h>
#include <wx/combobox.h>
#include <wx/frame.h>
#include <wx/grid.h>
#include <wx/init.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

#include <iostream>
#include <string>
#include <string_view>

namespace {

class FocusTestApp : public wxApp {
public:
  // Initializes the minimal wx application used by the focus utility test.
  bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(FocusTestApp);

class CustomTextEditor : public wxTextCtrl {
public:
  // Creates a custom text editor used to verify derived editable controls.
  CustomTextEditor(wxWindow *parent, wxWindowID id) : wxTextCtrl(parent, id) {}
};

class WxAppScope {
public:
  // Starts a GUI-capable wxWidgets application lifetime for widget creation.
  WxAppScope() {
    int argc = 0;
    char **argv = nullptr;
    started_ = wxEntryStart(argc, argv);
    if (started_ && wxTheApp)
      initialized_ = wxTheApp->CallOnInit();
  }

  // Cleans up the wxWidgets application lifetime started for the test.
  ~WxAppScope() {
    if (started_)
      wxEntryCleanup();
  }

  // Reports whether wxWidgets is ready for GUI widget creation.
  bool IsOk() const { return started_ && initialized_; }

private:
  bool started_ = false;
  bool initialized_ = false;
};

struct CheckContext {
  const wxWindow *window = nullptr;
  const wxFrame *frame = nullptr;
  const wxPanel *panel = nullptr;
  const wxGrid *grid = nullptr;
  const wxWindow *editor = nullptr;
  bool testedEditorChild = false;
};

// Returns a portable diagnostic name for a wxWidgets runtime class.
std::string RuntimeClassName(const wxWindow *window) {
  if (!window)
    return "<null>";
  return wxString(window->GetClassInfo()->GetClassName()).ToStdString();
}

// Reports an actionable diagnostic when a focus-fixture check fails.
bool CheckResult(std::string_view caseName, std::string_view checkName,
                 bool expected, bool actual, const CheckContext &context) {
  if (actual == expected)
    return true;

  const auto *textEntry = dynamic_cast<const wxTextEntry *>(context.window);
  const auto *combo = dynamic_cast<const wxComboBox *>(context.window);
  std::cerr
      << "EditableFocusUtils case=" << caseName << " check=" << checkName
      << " expected=" << expected << " actual=" << actual
      << " class=" << RuntimeClassName(context.window) << " text_editable="
      << (textEntry ? (textEntry->IsEditable() ? "true" : "false") : "n/a")
      << " combo_readonly_style="
      << (combo ? (combo->HasFlag(wxCB_READONLY) ? "true" : "false") : "n/a")
      << " grid_can_enable="
      << (context.grid
              ? (context.grid->CanEnableCellControl() ? "true" : "false")
              : "n/a")
      << " grid_editor_shown="
      << (context.grid
              ? (context.grid->IsCellEditControlShown() ? "true" : "false")
              : "n/a")
      << " frame_shown="
      << (context.frame && context.frame->IsShown() ? "true" : "false")
      << " panel_shown="
      << (context.panel && context.panel->IsShown() ? "true" : "false")
      << " grid_shown="
      << (context.grid && context.grid->IsShown() ? "true" : "false")
      << " editor_shown="
      << (context.editor && context.editor->IsShown() ? "true" : "false")
      << " tested_pointer="
      << (context.testedEditorChild ? "editor-child" : "control") << '\n';
  return false;
}

// Checks the focus classification and reports its complete fixture state.
bool CheckEditableFocus(std::string_view caseName, bool expected,
                        const CheckContext &context) {
  return CheckResult(caseName, "editable-focus", expected,
                     gui::IsEditableWidgetFocused(context.window), context);
}

} // namespace

// Verifies editable focus detection across native wxWidgets control types.
int main() {
  WxAppScope app;
  if (!app.IsOk()) {
    std::cerr << "EditableFocusUtils skipped because no GUI display is "
                 "available\n";
    return 77;
  }

  bool passed = true;
  wxFrame frame(nullptr, wxID_ANY, "focus-test");
  wxPanel panel(&frame);
  const CheckContext base{.frame = &frame, .panel = &panel};

  passed &= CheckEditableFocus("null", false, base);

  wxTextCtrl textCtrl(&panel, wxID_ANY);
  passed &= CheckEditableFocus(
      "editable-text", true,
      {.window = &textCtrl, .frame = &frame, .panel = &panel});

  wxTextCtrl readonlyText(&panel, wxID_ANY);
  readonlyText.SetEditable(false);
  passed &= CheckEditableFocus(
      "readonly-text", false,
      {.window = &readonlyText, .frame = &frame, .panel = &panel});

  wxComboBox editableCombo(&panel, wxID_ANY, "", wxDefaultPosition,
                           wxDefaultSize, 0, nullptr, wxCB_DROPDOWN);
  const CheckContext editableComboContext{
      .window = &editableCombo, .frame = &frame, .panel = &panel};
  passed &=
      CheckResult("editable-combo", "readonly-style", false,
                  editableCombo.HasFlag(wxCB_READONLY), editableComboContext);
  passed &= CheckEditableFocus("editable-combo", true, editableComboContext);

  wxComboBox readonlyCombo(&panel, wxID_ANY, "", wxDefaultPosition,
                           wxDefaultSize, 0, nullptr, wxCB_READONLY);
  const CheckContext readonlyComboContext{
      .window = &readonlyCombo, .frame = &frame, .panel = &panel};
  passed &=
      CheckResult("readonly-combo", "readonly-style", true,
                  readonlyCombo.HasFlag(wxCB_READONLY), readonlyComboContext);
  passed &= CheckEditableFocus("readonly-combo", false, readonlyComboContext);

  wxSpinCtrl spinCtrl(&panel, wxID_ANY);
  passed &= CheckEditableFocus(
      "spin", true, {.window = &spinCtrl, .frame = &frame, .panel = &panel});

  wxSpinCtrlDouble spinCtrlDouble(&panel, wxID_ANY);
  passed &= CheckEditableFocus(
      "spin-double", true,
      {.window = &spinCtrlDouble, .frame = &frame, .panel = &panel});

  CustomTextEditor customEditor(&panel, wxID_ANY);
  passed &= CheckEditableFocus(
      "derived-text", true,
      {.window = &customEditor, .frame = &frame, .panel = &panel});

  wxGrid grid(&panel, wxID_ANY);
  grid.CreateGrid(1, 1);
  grid.SetGridCursor(0, 0);
  const CheckContext gridContext{
      .window = &grid, .frame = &frame, .panel = &panel, .grid = &grid};
  passed &= CheckResult("grid-inactive", "can-enable", true,
                        grid.CanEnableCellControl(), gridContext);
  passed &= CheckResult("grid-inactive", "editor-shown", false,
                        grid.IsCellEditControlShown(), gridContext);
  passed &= CheckEditableFocus("grid-inactive", false, gridContext);

  grid.EnableCellEditControl();
  wxGridCellEditor *cellEditor = grid.GetCellEditor(0, 0);
  wxWindow *editorWindow = cellEditor ? cellEditor->GetWindow() : nullptr;
  const CheckContext activeGridContext{.window = &grid,
                                       .frame = &frame,
                                       .panel = &panel,
                                       .grid = &grid,
                                       .editor = editorWindow};
  passed &= CheckResult("grid-active", "editor-shown", true,
                        grid.IsCellEditControlShown(), activeGridContext);
  passed &= CheckEditableFocus("grid-active", true, activeGridContext);
  passed &= CheckResult("grid-editor-child", "editor-available", true,
                        editorWindow != nullptr, activeGridContext);
  if (editorWindow) {
    passed &= CheckEditableFocus("grid-editor-child", true,
                                 {.window = editorWindow,
                                  .frame = &frame,
                                  .panel = &panel,
                                  .grid = &grid,
                                  .editor = editorWindow,
                                  .testedEditorChild = true});
  }
  if (cellEditor)
    cellEditor->DecRef();

  grid.DisableCellEditControl();
  passed &= CheckResult("grid-disabled", "editor-shown", false,
                        grid.IsCellEditControlShown(), gridContext);
  passed &= CheckEditableFocus("grid-disabled", false, gridContext);

  wxWindow plainWindow(&panel, wxID_ANY);
  passed &= CheckEditableFocus(
      "plain-window", false,
      {.window = &plainWindow, .frame = &frame, .panel = &panel});

  return passed ? 0 : 1;
}
