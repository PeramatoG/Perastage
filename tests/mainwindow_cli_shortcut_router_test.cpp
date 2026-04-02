#include "mainwindow_cli_shortcut_router.h"

#include <cassert>

int main() {
  {
    const gui::CliShortcutRouteContext context{
        .hasModifiers = false,
        .focusInCliInput = false,
        .focusInEditableText = false,
        .cliHasTypedContent = false,
    };
    const auto result = gui::RouteCliShortcut('P', context);
    assert(result.shouldFocusCli);
    assert(result.prefill == "pos ");
  }

  {
    const gui::CliShortcutRouteContext context{
        .hasModifiers = false,
        .focusInCliInput = true,
        .focusInEditableText = false,
        .cliHasTypedContent = false,
    };
    const auto result = gui::RouteCliShortcut('R', context);
    assert(!result.shouldFocusCli);
    assert(result.prefill.empty());
  }

  {
    const gui::CliShortcutRouteContext context{
        .hasModifiers = false,
        .focusInCliInput = false,
        .focusInEditableText = true,
        .cliHasTypedContent = false,
    };
    const auto result = gui::RouteCliShortcut('P', context);
    assert(!result.shouldFocusCli);
    assert(result.prefill.empty());
  }

  {
    const gui::CliShortcutRouteContext context{
        .hasModifiers = false,
        .focusInCliInput = false,
        .focusInEditableText = false,
        .cliHasTypedContent = true,
    };
    const auto result = gui::RouteCliShortcut('R', context);
    assert(result.shouldFocusCli);
    assert(result.prefill.empty());
  }

  return 0;
}
