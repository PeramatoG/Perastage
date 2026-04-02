#pragma once

#include <optional>
#include <string>
#include <vector>

namespace gui {

enum class ShortcutScope {
  Global,
  Viewer2D,
  Viewer3D,
  Cli,
};

enum class ShortcutFocusPolicy {
  AllowInEditableWidgets,
  BlockInEditableWidgets,
};

enum class ShortcutAction {
  FitView,
  CliPrefillPos,
  CliPrefillRot,
  CliPrefillFixture,
};

struct ShortcutDefinition {
  int keyCode = 0;
  ShortcutAction action = ShortcutAction::FitView;
  std::string ownerModule;
  ShortcutScope scope = ShortcutScope::Global;
  ShortcutFocusPolicy focusPolicy = ShortcutFocusPolicy::BlockInEditableWidgets;
  int priority = 0;
};

struct ShortcutExecutionContext {
  bool hasModifiers = false;
  bool focusInEditableText = false;
  bool focusInCliInput = false;
  bool cliHasTypedContent = false;
  bool focusInViewer2D = false;
  bool focusInViewer3D = false;
};

struct ShortcutExecutionDecision {
  ShortcutAction action = ShortcutAction::FitView;
  std::string cliPrefill;
};

const std::vector<ShortcutDefinition> &GetShortcutRegistry();
std::vector<std::string> ValidateShortcutRegistry();
bool CanExecuteShortcut(const ShortcutDefinition &shortcut,
                        const ShortcutExecutionContext &context);
std::optional<ShortcutExecutionDecision>
ResolveShortcut(int keyCode, const ShortcutExecutionContext &context);

} // namespace gui
