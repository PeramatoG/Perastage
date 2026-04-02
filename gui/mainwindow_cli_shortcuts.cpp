#include "mainwindow.h"

#include "consolepanel.h"
#include "shortcut_registry.h"

#include <wx/combobox.h>
#include <wx/grid.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

namespace {

bool IsChildOrSame(const wxWindow *root, const wxWindow *window) {
  if (!root || !window)
    return false;
  const wxWindow *current = window;
  while (current) {
    if (current == root)
      return true;
    current = current->GetParent();
  }
  return false;
}

} // namespace

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
    if (auto *grid = dynamic_cast<const wxGrid *>(current);
        grid && grid->IsCellEditControlShown()) {
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

bool MainWindow::ApplyShortcutDecision(
    const gui::ShortcutExecutionDecision &decision) {
  switch (decision.action) {
  case gui::ShortcutAction::FitView:
    return ApplyFitShortcut();
  case gui::ShortcutAction::CliPrefillPos:
  case gui::ShortcutAction::CliPrefillRot:
  case gui::ShortcutAction::CliPrefillFixture:
    FocusConsoleForQuickCommand(wxString::FromUTF8(decision.cliPrefill));
    return true;
  }
  return false;
}

void MainWindow::OnGlobalCharHook(wxKeyEvent &event) {
  const wxWindow *focus = FindFocus();
  const gui::ShortcutExecutionContext context{
      .hasModifiers =
          event.ControlDown() || event.AltDown() || event.MetaDown(),
      .focusInEditableText = IsEditableTextWidgetOrChild(focus),
      .focusInCliInput = consolePanel && consolePanel->IsInputWidgetOrChild(focus),
      .cliHasTypedContent = consolePanel && consolePanel->InputHasTypedContent(),
      .focusInViewer2D =
          IsChildOrSame(viewport2DPanel, focus) ||
          IsChildOrSame(viewport2DRenderPanel, focus),
      .focusInViewer3D = IsChildOrSame(viewportPanel, focus),
  };

  const auto decision = gui::ResolveShortcut(event.GetKeyCode(), context);
  if (!decision.has_value() || !ApplyShortcutDecision(*decision)) {
    event.Skip();
    return;
  }
}
