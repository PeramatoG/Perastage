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

class CustomTextEditor : public wxTextCtrl {
public:
  CustomTextEditor(wxWindow *parent, wxWindowID id)
      : wxTextCtrl(parent, id) {}
};

bool Expect(bool condition) { return condition; }

} // namespace

int main() {
  wxInitializer initializer;
  if (!initializer.IsOk())
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
