#include "mainwindow_cli_shortcut_router.h"

#include "shortcut_registry.h"

namespace gui {

CliShortcutRouteResult RouteCliShortcut(int keyCode,
                                        const CliShortcutRouteContext &context) {
  const ShortcutExecutionContext shortcutContext{
      .hasModifiers = context.hasModifiers,
      .focusInEditableText = context.focusInEditableText,
      .focusInCliInput = context.focusInCliInput,
      .cliHasTypedContent = context.cliHasTypedContent,
      .focusInViewer2D = false,
      .focusInViewer3D = false,
  };

  CliShortcutRouteResult result;
  const auto decision = ResolveShortcut(keyCode, shortcutContext);
  if (!decision.has_value())
    return result;

  switch (decision->action) {
  case ShortcutAction::CliPrefillPos:
  case ShortcutAction::CliPrefillRot:
  case ShortcutAction::CliPrefillFixture:
    result.shouldFocusCli = true;
    result.prefill = decision->cliPrefill;
    break;
  default:
    break;
  }

  return result;
}

} // namespace gui
