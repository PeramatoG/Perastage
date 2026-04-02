#include "shortcut_registry.h"

#include <iostream>

namespace {

bool Expect(bool condition, const char *message) {
  if (condition)
    return true;
  std::cerr << "FAILED: " << message << '\n';
  return false;
}

} // namespace

int main() {
  bool ok = true;

  const auto errors = gui::ValidateShortcutRegistry();
  ok &= Expect(errors.empty(), "registry should not have scope collisions");

  const gui::ShortcutExecutionContext viewer2dContext{
      .focusInViewer2D = true,
  };
  const auto fit2d = gui::ResolveShortcut('z', viewer2dContext);
  ok &= Expect(fit2d.has_value(), "fit should resolve in viewer2d scope");
  ok &= Expect(fit2d && fit2d->action == gui::ShortcutAction::FitView,
               "fit action should be selected");

  const gui::ShortcutExecutionContext cliContext{
      .cliHasTypedContent = false,
  };
  const auto pos = gui::ResolveShortcut('p', cliContext);
  ok &= Expect(pos.has_value(), "pos shortcut should resolve in cli scope");
  ok &= Expect(pos && pos->cliPrefill == "pos ", "pos shortcut prefill mismatch");

  const gui::ShortcutExecutionContext typedCliContext{
      .cliHasTypedContent = true,
  };
  const auto rot = gui::ResolveShortcut('r', typedCliContext);
  ok &= Expect(rot.has_value(), "rot shortcut should resolve");
  ok &= Expect(rot && rot->cliPrefill.empty(),
               "rot should not force prefill when CLI has typed content");


  const gui::ShortcutExecutionContext tableContext{};
  const auto fixturesTab = gui::ResolveShortcut('1', tableContext);
  ok &= Expect(fixturesTab && fixturesTab->action == gui::ShortcutAction::SelectFixturesTab,
               "1 should resolve fixtures tab action");

  const gui::ShortcutExecutionContext viewer3dContext{
      .focusInViewer3D = true,
  };
  const auto numpadTop = gui::ResolveShortcut(gui::kShortcutKeyNumpad7, viewer3dContext);
  ok &= Expect(numpadTop && numpadTop->action == gui::ShortcutAction::ViewportTop,
               "NumPad7 should resolve top view in viewer3d scope");

  const gui::ShortcutExecutionContext editableContext{
      .focusInEditableText = true,
  };
  const auto blocked = gui::ResolveShortcut('f', editableContext);
  ok &= Expect(!blocked.has_value(), "shortcuts should be blocked in editable widgets");

  return ok ? 0 : 1;
}
