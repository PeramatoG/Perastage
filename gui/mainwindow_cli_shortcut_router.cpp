#include "mainwindow_cli_shortcut_router.h"

#include <cctype>

namespace gui {

CliShortcutRouteResult RouteCliShortcut(int keyCode,
                                        const CliShortcutRouteContext &context) {
  CliShortcutRouteResult result;

  if (context.hasModifiers || context.focusInCliInput ||
      context.focusInEditableText) {
    return result;
  }

  const int normalizedKey = std::toupper(keyCode);
  if (normalizedKey == 'P') {
    result.shouldFocusCli = true;
    if (!context.cliHasTypedContent)
      result.prefill = "pos ";
  } else if (normalizedKey == 'R') {
    result.shouldFocusCli = true;
    if (!context.cliHasTypedContent)
      result.prefill = "rot ";
  } else if (normalizedKey == 'F') {
    result.shouldFocusCli = true;
    result.prefill = "fixture ";
  }

  return result;
}

} // namespace gui
