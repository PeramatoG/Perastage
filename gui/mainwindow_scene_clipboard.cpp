#include "mainwindow.h"

#include "configmanager.h"
#include "consolepanel.h"
#include "editable_focus_utils.h"
#include "fixturetablepanel.h"
#include "hoisttablepanel.h"
#include "json.hpp"
#include "project_fixture_identity.h"
#include "sceneobjecttablepanel.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <wx/window.h>

namespace {

// Reads fixture label overrides as opaque JSON values keyed by fixture UUID.
std::unordered_map<std::string, std::string>
ReadFixtureLabelOverrides(const ConfigManager &config) {
  std::unordered_map<std::string, std::string> overrides;
  const auto raw = config.GetValue(project_identity::kFixtureLabelOverridesConfigKey);
  if (!raw)
    return overrides;
  const auto parsed = nlohmann::json::parse(*raw, nullptr, false);
  if (!parsed.is_object())
    return overrides;
  for (auto it = parsed.begin(); it != parsed.end(); ++it)
    overrides.emplace(it.key(), it.value().dump());
  return overrides;
}

// Writes fixture label overrides without interpreting their renderer-owned schema.
void WriteFixtureLabelOverrides(
    ConfigManager &config,
    const std::unordered_map<std::string, std::string> &overrides) {
  nlohmann::json serialized = nlohmann::json::object();
  for (const auto &[uuid, rawValue] : overrides) {
    const auto value = nlohmann::json::parse(rawValue, nullptr, false);
    if (!value.is_discarded())
      serialized[uuid] = value;
  }
  if (serialized.empty())
    config.RemoveKey(project_identity::kFixtureLabelOverridesConfigKey);
  else
    config.SetValue(project_identity::kFixtureLabelOverridesConfigKey,
                    serialized.dump());
}

// Copies the central application selection into the domain selection type.
SelectionState BuildSelection(const ConfigManager &config) {
  SelectionState selection;
  selection.SetSelectedFixtures(config.GetSelectedFixtures());
  selection.SetSelectedTrusses(config.GetSelectedTrusses());
  selection.SetSelectedSupports(config.GetSelectedSupports());
  selection.SetSelectedSceneObjects(config.GetSelectedSceneObjects());
  return selection;
}

// Reports whether native text or cell clipboard handling owns the current focus.
bool EditableControlOwnsClipboard() {
  return gui::IsEditableWidgetFocused(wxWindow::FindFocus());
}

} // namespace

// Captures the supported central scene selection without mutating the project.
void MainWindow::OnCopy(wxCommandEvent &event) {
  if (EditableControlOwnsClipboard()) {
    event.Skip();
    return;
  }
  ConfigManager &config = guiConfigServices->LegacyConfigManager();
  const SelectionState selection = BuildSelection(config);
  const auto overrides = ReadFixtureLabelOverrides(config);
  if (sceneClipboard.Capture(config.GetScene(), selection,
                             sceneClipboardEpoch, overrides) &&
      consolePanel) {
    consolePanel->AppendMessage("Copied selected scene elements");
  }
}

// Captures and removes the supported central scene selection as one edit.
void MainWindow::OnCut(wxCommandEvent &event) {
  if (EditableControlOwnsClipboard()) {
    event.Skip();
    return;
  }
  ConfigManager &config = guiConfigServices->LegacyConfigManager();
  SelectionState selection = BuildSelection(config);
  auto overrides = ReadFixtureLabelOverrides(config);
  if (!sceneClipboard.Capture(config.GetScene(), selection,
                              sceneClipboardEpoch, overrides)) {
    return;
  }
  config.PushUndoState("cut scene elements");
  const auto result = sceneClipboard.Cut(config.GetScene(), selection,
                                         sceneClipboardEpoch, &overrides);
  if (!result.changed)
    return;
  WriteFixtureLabelOverrides(config, overrides);
  config.SetSelectedFixtures({});
  config.SetSelectedTrusses({});
  config.SetSelectedSupports({});
  config.SetSelectedSceneObjects({});
  RefreshAfterSceneChange();
  if (consolePanel)
    consolePanel->AppendMessage("Cut selected scene elements");
}

// Pastes clipboard values into the current scene and selects the new instances.
void MainWindow::OnPaste(wxCommandEvent &event) {
  if (EditableControlOwnsClipboard()) {
    event.Skip();
    return;
  }
  if (!sceneClipboard.CanPaste(sceneClipboardEpoch)) {
    return;
  }
  if ((viewport2DPanel && viewport2DPanel->IsContinuousPlacementActive()) ||
      (viewportPanel && viewportPanel->IsContinuousPlacementActive())) {
    return;
  }
  ConfigManager &config = guiConfigServices->LegacyConfigManager();
  auto overrides = ReadFixtureLabelOverrides(config);
  config.PushUndoState("paste scene elements");
  const auto result = sceneClipboard.Paste(config.GetScene(),
                                           sceneClipboardEpoch, &overrides);
  if (!result.changed)
    return;

  std::vector<std::string> fixtures;
  std::vector<std::string> trusses;
  std::vector<std::string> supports;
  std::vector<std::string> objects;
  for (const auto &node : result.nodes) {
    switch (node.type) {
    case MvrNodeType::Fixture: fixtures.push_back(node.uuid); break;
    case MvrNodeType::Truss: trusses.push_back(node.uuid); break;
    case MvrNodeType::Support: supports.push_back(node.uuid); break;
    case MvrNodeType::SceneObject: objects.push_back(node.uuid); break;
    case MvrNodeType::GroupObject: break;
    }
  }
  WriteFixtureLabelOverrides(config, overrides);
  config.SetSelectedFixtures(fixtures);
  config.SetSelectedTrusses(trusses);
  config.SetSelectedSupports(supports);
  config.SetSelectedSceneObjects(objects);
  RefreshAfterSceneChange();
  if (fixturePanel) fixturePanel->SelectByUuid(fixtures, false);
  if (trussPanel) trussPanel->SelectByUuid(trusses, false);
  if (hoistPanel) hoistPanel->SelectByUuid(supports, false);
  if (sceneObjPanel) sceneObjPanel->SelectByUuid(objects, false);
  if (consolePanel)
    consolePanel->AppendMessage("Pasted scene elements");
}

// Clears clipboard data after the active project scene is replaced.
void MainWindow::InvalidateSceneClipboard() {
  sceneClipboard.Clear();
  ++sceneClipboardEpoch;
  if (sceneClipboardEpoch == 0)
    sceneClipboardEpoch = 1;
}
