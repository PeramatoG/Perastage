#include "mainwindow.h"

#include "configmanager.h"
#include "consolepanel.h"
#include "editable_focus_utils.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "json.hpp"
#include "project_fixture_identity.h"
#include "sceneobjecttablepanel.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <wx/window.h>

namespace {

struct PasteBeforeState {
  MvrScene scene;
  SelectionState selection;
  ConfigManager::DirtyState dirty;
  std::optional<std::string> labelOverrides;
};

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

// Converts a selectable MVR node type into the shared placement type.
ContinuousPlacementType PlacementTypeForNode(MvrNodeType type) {
  switch (type) {
  case MvrNodeType::Fixture: return ContinuousPlacementType::Fixture;
  case MvrNodeType::Truss: return ContinuousPlacementType::Truss;
  case MvrNodeType::Support: return ContinuousPlacementType::Support;
  case MvrNodeType::SceneObject: return ContinuousPlacementType::SceneObject;
  case MvrNodeType::GroupObject: return ContinuousPlacementType::None;
  }
  return ContinuousPlacementType::None;
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
  const bool singleItem = sceneClipboard.GetPayload().items.size() == 1;
  auto before = std::make_shared<PasteBeforeState>(PasteBeforeState{
      config.GetScene(), BuildSelection(config), config.CaptureDirtyState(),
      config.GetValue(project_identity::kFixtureLabelOverridesConfigKey)});
  if (singleItem)
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
  if (!singleItem)
    config.RestoreDirtyState(before->dirty);
  config.SetSelectedFixtures(fixtures);
  config.SetSelectedTrusses(trusses);
  config.SetSelectedSupports(supports);
  config.SetSelectedSceneObjects(objects);
  RefreshAfterSceneChange();
  if (fixturePanel) fixturePanel->SelectByUuid(fixtures, false);
  if (trussPanel) trussPanel->SelectByUuid(trusses, false);
  if (hoistPanel) hoistPanel->SelectByUuid(supports, false);
  if (sceneObjPanel) sceneObjPanel->SelectByUuid(objects, false);
  if (result.nodes.size() == 1) {
    const ContinuousPlacementType type =
        PlacementTypeForNode(result.nodes.front().type);
    const std::string provisionalUuid = result.nodes.front().uuid;
    auto cloneFactory = [this]() -> std::string {
      ConfigManager &activeConfig = guiConfigServices->LegacyConfigManager();
      auto activeOverrides = ReadFixtureLabelOverrides(activeConfig);
      const auto next = sceneClipboard.Paste(activeConfig.GetScene(),
                                             sceneClipboardEpoch,
                                             &activeOverrides);
      if (!next.changed || next.nodes.size() != 1)
        return {};
      WriteFixtureLabelOverrides(activeConfig, activeOverrides);
      return next.nodes.front().uuid;
    };
    const bool use2D = viewport2DPanel && viewport2DPanel->IsShownOnScreen() &&
                       (viewport2DPanel->HasFocus() || !viewportPanel ||
                        !viewportPanel->IsShownOnScreen());
    if (use2D) {
      viewport2DPanel->BeginClipboardContinuousPlacement(
          type, provisionalUuid, std::move(cloneFactory));
    } else {
      if (!viewportPanel || !viewportPanel->IsShownOnScreen())
        Ensure2DViewportAvailable();
      if (viewportPanel && viewportPanel->IsShownOnScreen())
        viewportPanel->BeginClipboardContinuousPlacement(
            type, provisionalUuid, std::move(cloneFactory));
      else if (viewport2DPanel)
        viewport2DPanel->BeginClipboardContinuousPlacement(
            type, provisionalUuid, std::move(cloneFactory));
    }
  } else {
    scene_grouping::ObjectSelection batch{fixtures, trusses, supports, objects};
    auto restoreBefore = [this, before]() {
      ConfigManager &active = guiConfigServices->LegacyConfigManager();
      active.GetScene() = before->scene;
      active.SetSelectedFixtures(before->selection.GetSelectedFixtures());
      active.SetSelectedTrusses(before->selection.GetSelectedTrusses());
      active.SetSelectedSupports(before->selection.GetSelectedSupports());
      active.SetSelectedSceneObjects(
          before->selection.GetSelectedSceneObjects());
      if (before->labelOverrides)
        active.SetValue(project_identity::kFixtureLabelOverridesConfigKey,
                        *before->labelOverrides);
      else
        active.RemoveKey(project_identity::kFixtureLabelOverridesConfigKey);
      active.RestoreDirtyState(before->dirty);
    };
    auto confirm = [this, before]() {
      ConfigManager &active = guiConfigServices->LegacyConfigManager();
      const MvrScene finalScene = active.GetScene();
      const SelectionState finalSelection = BuildSelection(active);
      const auto finalOverrides = active.GetValue(
          project_identity::kFixtureLabelOverridesConfigKey);
      active.GetScene() = before->scene;
      active.SetSelectedFixtures(before->selection.GetSelectedFixtures());
      active.SetSelectedTrusses(before->selection.GetSelectedTrusses());
      active.SetSelectedSupports(before->selection.GetSelectedSupports());
      active.SetSelectedSceneObjects(before->selection.GetSelectedSceneObjects());
      if (before->labelOverrides)
        active.SetValue(project_identity::kFixtureLabelOverridesConfigKey,
                        *before->labelOverrides);
      else
        active.RemoveKey(project_identity::kFixtureLabelOverridesConfigKey);
      active.RestoreDirtyState(before->dirty);
      active.PushUndoState("paste scene elements");
      active.GetScene() = finalScene;
      active.SetSelectedFixtures(finalSelection.GetSelectedFixtures());
      active.SetSelectedTrusses(finalSelection.GetSelectedTrusses());
      active.SetSelectedSupports(finalSelection.GetSelectedSupports());
      active.SetSelectedSceneObjects(finalSelection.GetSelectedSceneObjects());
      if (finalOverrides)
        active.SetValue(project_identity::kFixtureLabelOverridesConfigKey,
                        *finalOverrides);
      RefreshAfterSceneChange();
    };
    auto cancel = [this, restoreBefore]() {
      restoreBefore();
      RefreshAfterSceneChange();
    };
    Ensure2DViewportAvailable();
    if (viewport2DPanel)
      viewport2DPanel->BeginClipboardBatchPlacement(
          batch, std::move(confirm), std::move(cancel));
  }
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
