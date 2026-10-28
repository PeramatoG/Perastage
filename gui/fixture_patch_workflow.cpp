/* This file is part of Perastage. */
#include "fixture_patch_workflow.h"
#include "autopatcher.h"
#include "configmanager.h"
#include "consolepanel.h"

namespace fixture_patch_workflow {
bool AutoPatch(ConfigManager &config) {
  auto candidate = config.GetScene();
  const auto selected = config.GetSelectedFixtures();
  const auto result = selected.empty() ? AutoPatcher::AutoPatch(candidate)
      : AutoPatcher::AutoPatchSelection(candidate, selected);
  if (!result) {
    if (ConsolePanel::Instance())
      ConsolePanel::Instance()->AppendMessage(wxString::FromUTF8(result.detail));
    return false;
  }
  config.PushUndoState("auto patch");
  config.GetScene().fixtures = std::move(candidate.fixtures);
  return true;
}
}
