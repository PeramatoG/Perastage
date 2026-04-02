#include "mainwindow.h"

#include "consolepanel.h"
#include "mainwindow_cli_shortcut_router.h"

#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

bool MainWindow::IsEditableTextWidgetOrChild(const wxWindow *window) const {
  const wxWindow *current = window;
  while (current) {
    if (dynamic_cast<const wxTextCtrl *>(current))
      return true;
    if (auto *combo = dynamic_cast<const wxComboBox *>(current);
        combo && combo->IsEditable()) {
      return true;
    }
    if (dynamic_cast<const wxSpinCtrl *>(current) ||
        dynamic_cast<const wxSpinCtrlDouble *>(current)) {
      return true;
    }
    if (current->IsEditable()) {
      return true;
    }
    current = current->GetParent();
  }
  return false;
}

void MainWindow::FocusConsoleForQuickCommand(const wxString &prefill) {
  if (!consolePanel)
    return;

  if (auiManager) {
    auto &pane = auiManager->GetPane("Console");
    if (pane.IsOk() && !pane.IsShown()) {
      pane.Show(true);
      auiManager->Update();
      UpdateViewMenuChecks();
    }
  }

  consolePanel->FocusInputWithOptionalPrefill(prefill);
}

void MainWindow::OnGlobalCharHook(wxKeyEvent &event) {
  if (!consolePanel) {
    event.Skip();
    return;
  }

  const gui::CliShortcutRouteContext context{
      .hasModifiers =
          event.ControlDown() || event.AltDown() || event.MetaDown(),
      .focusInCliInput = consolePanel->IsInputWidgetOrChild(FindFocus()),
      .focusInEditableText = IsEditableTextWidgetOrChild(FindFocus()),
      .cliHasTypedContent = consolePanel->InputHasTypedContent(),
  };

  const gui::CliShortcutRouteResult route =
      gui::RouteCliShortcut(event.GetKeyCode(), context);
  if (!route.shouldFocusCli) {
    event.Skip();
    return;
  }

  FocusConsoleForQuickCommand(wxString::FromUTF8(route.prefill));
}
