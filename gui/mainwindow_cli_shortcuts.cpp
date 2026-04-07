#include "mainwindow.h"

#include "consolepanel.h"
#include "editable_focus_utils.h"
#include "shortcut_registry.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"

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
    ApplyViewportShortcut(Viewer2DView::Front);
    return true;
  case gui::ShortcutAction::ViewportSide:
    ApplyViewportShortcut(Viewer2DView::Side);
    return true;
  case gui::ShortcutAction::ViewportTop:
    ApplyViewportShortcut(Viewer2DView::Top);
    return true;
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
      .focusInEditableText = gui::IsEditableWidgetFocused(focus),
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
