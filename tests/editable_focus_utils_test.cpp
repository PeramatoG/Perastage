#include "editable_focus_utils.h"

#include <wx/app.h>
#include <wx/combobox.h>
#include <wx/frame.h>
#include <wx/grid.h>
#include <wx/init.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

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
  CustomTextEditor(wxWindow *parent, wxWindowID id)
      : wxTextCtrl(parent, id) {}
};

class WxAppScope {
public:
  // Starts a GUI-capable wxWidgets application lifetime for GTK widget creation.
  WxAppScope() {
    int argc = 0;
    char **argv = nullptr;
    started_ = wxEntryStart(argc, argv);
    if (started_ && wxTheApp)
      initialized_ = wxTheApp->CallOnInit();
  }

  // Cleans up the wxWidgets application lifetime started for the test.
  ~WxAppScope() {
    if (initialized_ && wxTheApp)
      wxTheApp->OnExit();
    if (started_)
      wxEntryCleanup();
  }

  // Reports whether wxWidgets is ready for GUI widget creation.
  bool IsOk() const { return started_ && initialized_; }

private:
  bool started_ = false;
  bool initialized_ = false;
};

// Returns the condition value with a named helper for breakpoint-friendly checks.
bool Expect(bool condition) { return condition; }

} // namespace

// Verifies editable focus detection across native wxWidgets control types.
int main() {
  WxAppScope app;
  if (!app.IsOk())
    return 1;

  wxFrame frame(nullptr, wxID_ANY, "focus-test");
  wxPanel panel(&frame);

  wxTextCtrl textCtrl(&panel, wxID_ANY);
  if (!Expect(gui::IsEditableWidgetFocused(&textCtrl)))
    return 1;

  wxComboBox editableCombo(&panel, wxID_ANY, "", wxDefaultPosition,
                           wxDefaultSize, 0, nullptr, wxCB_DROPDOWN);
  if (!Expect(gui::IsEditableWidgetFocused(&editableCombo)))
    return 1;

  wxComboBox readonlyCombo(&panel, wxID_ANY, "", wxDefaultPosition,
                           wxDefaultSize, 0, nullptr, wxCB_READONLY);
  if (!Expect(!gui::IsEditableWidgetFocused(&readonlyCombo)))
    return 1;

  wxSpinCtrl spinCtrl(&panel, wxID_ANY);
  if (!Expect(gui::IsEditableWidgetFocused(&spinCtrl)))
    return 1;

  CustomTextEditor customEditor(&panel, wxID_ANY);
  if (!Expect(gui::IsEditableWidgetFocused(&customEditor)))
    return 1;

  wxGrid grid(&panel, wxID_ANY);
  grid.CreateGrid(1, 1);
  grid.SetGridCursor(0, 0);
  grid.ShowCellEditControl();
  if (!Expect(gui::IsEditableWidgetFocused(&grid)))
    return 1;

  wxWindow plainWindow(&panel, wxID_ANY);
  if (!Expect(!gui::IsEditableWidgetFocused(&plainWindow)))
    return 1;

  return 0;
}
