#pragma once

#include <string>

namespace gui {

struct CliShortcutRouteContext {
  bool hasModifiers = false;
  bool focusInCliInput = false;
  bool focusInEditableText = false;
  bool cliHasTypedContent = false;
};

struct CliShortcutRouteResult {
  bool shouldFocusCli = false;
  std::string prefill;
};

CliShortcutRouteResult RouteCliShortcut(int keyCode,
                                        const CliShortcutRouteContext &context);

} // namespace gui
