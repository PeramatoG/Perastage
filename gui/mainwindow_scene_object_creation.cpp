#include "mainwindow.h"

#include <chrono>
#include <string>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "layer.h"
#include "sceneobject.h"
#include "sceneobjecttablepanel.h"
#include "viewer3dpanel.h"

namespace {

std::string EnsureCurrentLayerExists(ConfigManager &cfg, long long baseId) {
  auto &scene = cfg.GetScene();
  const std::string layerName = cfg.GetCurrentLayer();

  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == layerName)
      return layerName;
  }

  Layer layer;
  layer.uuid = wxString::Format("layer_%lld", baseId).ToStdString();
  layer.name = layerName;
  scene.layers[layer.uuid] = layer;
  return layerName;
}

} // namespace

void MainWindow::AddSceneObjects(
    const std::string &undoLabel, const std::string &baseName, long quantity,
    const std::function<void(SceneObject &, long)> &configureObject) {
  if (quantity <= 0)
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();
  cfg.PushUndoState(undoLabel);

  const auto baseIdRaw =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto baseId = static_cast<long long>(baseIdRaw);
  const std::string layerName = EnsureCurrentLayerExists(cfg, baseId);

  for (long i = 0; i < quantity; ++i) {
    SceneObject object;
    object.uuid = wxString::Format("uuid_%lld", baseId + i).ToStdString();
    object.name =
        quantity > 1 ? baseName + " " + std::to_string(i + 1) : baseName;
    object.layer = layerName;
    configureObject(object, i);
    scene.sceneObjects[object.uuid] = object;
  }
}

void MainWindow::RefreshAfterSceneObjectCreation() {
  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}
