#include "mainwindow.h"

#include "consolepanel.h"
#include "shortcut_registry.h"
#include "viewer3dpanel.h"

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

int NormalizeShortcutKeyForRegistry(int keyCode) {
  switch (keyCode) {
  case WXK_NUMPAD1:
    return gui::kShortcutKeyNumpad1;
  case WXK_NUMPAD3:
    return gui::kShortcutKeyNumpad3;
  case WXK_NUMPAD5:
    return gui::kShortcutKeyNumpad5;
  case WXK_NUMPAD7:
    return gui::kShortcutKeyNumpad7;
  default:
    return keyCode;
  }
}

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
  case gui::ShortcutAction::SelectFixturesTab:
  case gui::ShortcutAction::SelectTrussesTab:
  case gui::ShortcutAction::SelectSupportsTab:
  case gui::ShortcutAction::SelectObjectsTab: {
    int commandId = ID_Select_Fixtures;
    if (decision.action == gui::ShortcutAction::SelectTrussesTab)
      commandId = ID_Select_Trusses;
    else if (decision.action == gui::ShortcutAction::SelectSupportsTab)
      commandId = ID_Select_Supports;
    else if (decision.action == gui::ShortcutAction::SelectObjectsTab)
      commandId = ID_Select_Objects;

    wxCommandEvent event(wxEVT_MENU, commandId);
    return GetEventHandler()->ProcessEvent(event);
  }
  case gui::ShortcutAction::ViewportFront:
    if (viewportPanel) {
      viewportPanel->SetStandardView(Viewer2DView::Front);
      return true;
    }
    return false;
  case gui::ShortcutAction::ViewportSide:
    if (viewportPanel) {
      viewportPanel->SetStandardView(Viewer2DView::Side);
      return true;
    }
    return false;
  case gui::ShortcutAction::ViewportTop:
    if (viewportPanel) {
      viewportPanel->SetStandardView(Viewer2DView::Top);
      return true;
    }
    return false;
  case gui::ShortcutAction::ViewportReset3D:
    if (viewportPanel)
      return viewportPanel->ResetCameraToIsometric();
    return false;
  }
  return false;
}

void MainWindow::OnGlobalCharHook(wxKeyEvent &event) {
  if (!shortcutHandlingEnabled) {
    event.Skip();
    return;
  }
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

  const int normalizedKeyCode = NormalizeShortcutKeyForRegistry(event.GetKeyCode());
  const auto decision = gui::ResolveShortcut(normalizedKeyCode, context);
  if (!decision.has_value() || !ApplyShortcutDecision(*decision)) {
    event.Skip();
    return;
  }
}
