#include "shortcut_registry.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <tuple>

namespace gui {
namespace {

int NormalizeKeyCode(int keyCode) {
  if (keyCode >= 0 && keyCode <= 255)
    return std::toupper(static_cast<unsigned char>(keyCode));
  return keyCode;
}

std::optional<std::string> CliPrefillForAction(const ShortcutAction action,
                                               const bool cliHasTypedContent) {
  switch (action) {
  case ShortcutAction::CliPrefillPos:
    return cliHasTypedContent ? std::optional<std::string>{} : "pos ";
  case ShortcutAction::CliPrefillRot:
    return cliHasTypedContent ? std::optional<std::string>{} : "rot ";
  case ShortcutAction::CliPrefillFixture:
    return "fixture ";
  default:
    return std::nullopt;
  }
}

bool ScopeMatchesFocus(const ShortcutScope scope,
                      const ShortcutExecutionContext &context) {
  switch (scope) {
  case ShortcutScope::Global:
    return true;
  case ShortcutScope::Viewer2D:
    return context.focusInViewer2D;
  case ShortcutScope::Viewer3D:
    return context.focusInViewer3D;
  case ShortcutScope::Cli:
    return !context.focusInCliInput;
  }
  return false;
}

} // namespace

const std::vector<ShortcutDefinition> &GetShortcutRegistry() {
  static const std::vector<ShortcutDefinition> kRegistry = {
      ShortcutDefinition{.keyCode = 'Z',
                         .action = ShortcutAction::FitView,
                         .ownerModule = "viewer2d",
                         .scope = ShortcutScope::Viewer2D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 300},
      ShortcutDefinition{.keyCode = 'Z',
                         .action = ShortcutAction::FitView,
                         .ownerModule = "viewer3d",
                         .scope = ShortcutScope::Viewer3D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 300},
      ShortcutDefinition{.keyCode = 'P',
                         .action = ShortcutAction::CliPrefillPos,
                         .ownerModule = "cli",
                         .scope = ShortcutScope::Cli,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 200},
      ShortcutDefinition{.keyCode = 'R',
                         .action = ShortcutAction::CliPrefillRot,
                         .ownerModule = "cli",
                         .scope = ShortcutScope::Cli,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 200},
      ShortcutDefinition{.keyCode = 'F',
                         .action = ShortcutAction::CliPrefillFixture,
                         .ownerModule = "cli",
                         .scope = ShortcutScope::Cli,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 200},
      ShortcutDefinition{.keyCode = '1',
                         .action = ShortcutAction::SelectFixturesTab,
                         .ownerModule = "gui",
                         .scope = ShortcutScope::Global,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 150},
      ShortcutDefinition{.keyCode = '2',
                         .action = ShortcutAction::SelectTrussesTab,
                         .ownerModule = "gui",
                         .scope = ShortcutScope::Global,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 150},
      ShortcutDefinition{.keyCode = '3',
                         .action = ShortcutAction::SelectSupportsTab,
                         .ownerModule = "gui",
                         .scope = ShortcutScope::Global,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 150},
      ShortcutDefinition{.keyCode = '4',
                         .action = ShortcutAction::SelectObjectsTab,
                         .ownerModule = "gui",
                         .scope = ShortcutScope::Global,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 150},
      ShortcutDefinition{.keyCode = kShortcutKeyNumpad1,
                         .action = ShortcutAction::ViewportFront,
                         .ownerModule = "viewer2d",
                         .scope = ShortcutScope::Viewer2D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 250},
      ShortcutDefinition{.keyCode = kShortcutKeyNumpad3,
                         .action = ShortcutAction::ViewportSide,
                         .ownerModule = "viewer2d",
                         .scope = ShortcutScope::Viewer2D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 250},
      ShortcutDefinition{.keyCode = kShortcutKeyNumpad7,
                         .action = ShortcutAction::ViewportTop,
                         .ownerModule = "viewer2d",
                         .scope = ShortcutScope::Viewer2D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 250},
      ShortcutDefinition{.keyCode = kShortcutKeyNumpad1,
                         .action = ShortcutAction::ViewportFront,
                         .ownerModule = "viewer3d",
                         .scope = ShortcutScope::Viewer3D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 250},
      ShortcutDefinition{.keyCode = kShortcutKeyNumpad3,
                         .action = ShortcutAction::ViewportSide,
                         .ownerModule = "viewer3d",
                         .scope = ShortcutScope::Viewer3D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 250},
      ShortcutDefinition{.keyCode = kShortcutKeyNumpad7,
                         .action = ShortcutAction::ViewportTop,
                         .ownerModule = "viewer3d",
                         .scope = ShortcutScope::Viewer3D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 250},
      ShortcutDefinition{.keyCode = kShortcutKeyNumpad5,
                         .action = ShortcutAction::ViewportReset3D,
                         .ownerModule = "viewer3d",
                         .scope = ShortcutScope::Viewer3D,
                         .focusPolicy =
                             ShortcutFocusPolicy::BlockInEditableWidgets,
                         .priority = 250},
  };
  return kRegistry;
}

std::vector<std::string> ValidateShortcutRegistry() {
  std::vector<std::string> errors;
  std::map<std::tuple<ShortcutScope, int>, std::vector<size_t>> index;
  const auto &registry = GetShortcutRegistry();

  for (size_t i = 0; i < registry.size(); ++i) {
    const int normalizedKey = NormalizeKeyCode(registry[i].keyCode);
    index[{registry[i].scope, normalizedKey}].push_back(i);
  }

  for (const auto &[key, indices] : index) {
    if (indices.size() <= 1)
      continue;

    const auto &[scope, normalizedKey] = key;
    (void)scope;
    std::string details;
    for (size_t i = 0; i < indices.size(); ++i) {
      const ShortcutDefinition &shortcut = registry[indices[i]];
      if (!details.empty())
        details += ", ";
      details += shortcut.ownerModule;
    }

    const std::string keyLabel =
        (normalizedKey >= 32 && normalizedKey <= 126)
            ? ("'" + std::string(1, static_cast<char>(normalizedKey)) + "'")
            : ("code " + std::to_string(normalizedKey));
    errors.push_back("Shortcut collision for key " + keyLabel +
                     " in same scope. Owners: " + details);
  }

  return errors;
}

bool CanExecuteShortcut(const ShortcutDefinition &shortcut,
                        const ShortcutExecutionContext &context) {
  if (context.hasModifiers)
    return false;
  if (shortcut.focusPolicy == ShortcutFocusPolicy::BlockInEditableWidgets &&
      context.focusInEditableText) {
    return false;
  }
  return ScopeMatchesFocus(shortcut.scope, context);
}

std::optional<ShortcutExecutionDecision>
ResolveShortcut(int keyCode, const ShortcutExecutionContext &context) {
  const int normalizedKey = NormalizeKeyCode(keyCode);
  const auto &registry = GetShortcutRegistry();

  const ShortcutDefinition *best = nullptr;
  for (const ShortcutDefinition &shortcut : registry) {
    if (NormalizeKeyCode(shortcut.keyCode) != normalizedKey)
      continue;
    if (!CanExecuteShortcut(shortcut, context))
      continue;

    if (!best || shortcut.priority > best->priority)
      best = &shortcut;
  }

  if (!best)
    return std::nullopt;

  ShortcutExecutionDecision decision;
  decision.action = best->action;
  if (const auto prefill = CliPrefillForAction(best->action, context.cliHasTypedContent);
      prefill.has_value()) {
    decision.cliPrefill = *prefill;
  }
  return decision;
}

} // namespace gui
