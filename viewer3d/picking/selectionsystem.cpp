#include "selectionsystem.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "../../core/configmanager.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace {

struct ScreenRect {
  double minX = DBL_MAX;
  double minY = DBL_MAX;
  double maxX = -DBL_MAX;
  double maxY = -DBL_MAX;
};

std::unordered_set<std::string> SnapshotHiddenLayers(const ConfigManager &cfg) {
  return cfg.GetHiddenLayers();
}

bool IsLayerVisibleCached(const std::unordered_set<std::string> &hidden,
                         const std::string &layer) {
  if (layer.empty())
    return hidden.find(DEFAULT_LAYER_NAME) == hidden.end();
  return hidden.find(layer) == hidden.end();
}

bool IsFastInteractionModeEnabled(const ConfigManager &cfg) {
  return cfg.GetFloat("viewer3d_fast_interaction_mode") >= 0.5f;
}

std::string FormatMeters(float mm) {
  const float meters = mm / 1000.0f;
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2f", meters);
  return std::string(buffer);
}

} // namespace

void SelectionSystem::SetHighlightUuid(const std::string &uuid) {
  m_controller.m_highlightUuid = uuid;
}

void SelectionSystem::SetSelectedUuids(const std::vector<std::string> &uuids) {
  m_controller.m_selectedUuids.clear();
  for (const auto &u : uuids)
    m_controller.m_selectedUuids.insert(u);
}

bool SelectionSystem::GetFixtureLabelAt(int mouseX, int mouseY, int width,
                                        int height, wxString &outLabel,
                                        wxPoint &outPos,
                                        std::string *outUuid) {
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.m_cameraMoving && IsFastInteractionModeEnabled(cfg))
    return false;

  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  bool showName = cfg.GetFloat("label_show_name") != 0.0f;
  bool showId = cfg.GetFloat("label_show_id") != 0.0f;
  bool showDmx = cfg.GetFloat("label_show_dmx") != 0.0f;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();

  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;

  for (const auto &[uuid, f] : fixtures) {
    if (!IsLayerVisibleCached(hiddenLayers, f.layer))
      continue;
    auto bit = m_controller.m_fixtureBounds.find(uuid);
    if (bit == m_controller.m_fixtureBounds.end())
      continue;

    const Viewer3DController::BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
        wxString label;
        if (showName)
          label = f.instanceName.empty() ? wxString::FromUTF8(uuid)
                                         : wxString::FromUTF8(f.instanceName);
        if (showId) {
          if (!label.empty())
            label += "\n";
          label += "ID: " + wxString::Format("%d", f.fixtureId);
        }
        if (showDmx && !f.address.empty()) {
          if (!label.empty())
            label += "\n";
          label += wxString::FromUTF8(f.address);
        }
        if (label.empty())
          continue;
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = label;
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}

bool SelectionSystem::GetTrussLabelAt(int mouseX, int mouseY, int width,
                                      int height, wxString &outLabel,
                                      wxPoint &outPos,
                                      std::string *outUuid) {
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.m_cameraMoving && IsFastInteractionModeEnabled(cfg))
    return false;

  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  for (const auto &[uuid, t] : trusses) {
    if (!IsLayerVisibleCached(hiddenLayers, t.layer))
      continue;
    auto bit = m_controller.m_trussBounds.find(uuid);
    if (bit == m_controller.m_trussBounds.end())
      continue;

    const Viewer3DController::BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = t.name.empty() ? wxString::FromUTF8(uuid)
                                   : wxString::FromUTF8(t.name);
        float baseHeight = t.transform.o[2] - t.heightMm * 0.5f;
        std::string hStr = FormatMeters(baseHeight);
        bestLabel += wxString::Format("\nh = %s m", hStr.c_str());
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}

bool SelectionSystem::GetSceneObjectLabelAt(int mouseX, int mouseY, int width,
                                            int height, wxString &outLabel,
                                            wxPoint &outPos,
                                            std::string *outUuid) {
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.m_cameraMoving && IsFastInteractionModeEnabled(cfg))
    return false;

  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const auto &objs = SceneDataManager::Instance().GetSceneObjects();
  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  for (const auto &[uuid, o] : objs) {
    if (!IsLayerVisibleCached(hiddenLayers, o.layer))
      continue;
    auto bit = m_controller.m_objectBounds.find(uuid);
    if (bit == m_controller.m_objectBounds.end())
      continue;

    const Viewer3DController::BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = o.name.empty() ? wxString::FromUTF8(uuid)
                                   : wxString::FromUTF8(o.name);
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}

std::vector<std::string> SelectionSystem::GetFixturesInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  ConfigManager &cfg = ConfigManager::Get();
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);

  ScreenRect selectionRect;
  selectionRect.minX = std::max(0, std::min(x1, x2));
  selectionRect.maxX = std::min(width, std::max(x1, x2));
  selectionRect.minY = std::max(0, std::min(y1, y2));
  selectionRect.maxY = std::min(height, std::max(y1, y2));

  auto intersects = [&](const ScreenRect &rect) {
    return !(rect.maxX < selectionRect.minX || rect.minX > selectionRect.maxX ||
             rect.maxY < selectionRect.minY || rect.minY > selectionRect.maxY);
  };

  auto projectBounds = [&](const Viewer3DController::BoundingBox &bb, ScreenRect &rect) {
    rect = ScreenRect{};
    bool visible = false;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0)
          visible = true;
      }
    }
    return visible;
  };

  std::vector<std::string> selection;
  Viewer3DController::ViewFrustumSnapshot frustum{};
  std::copy(std::begin(viewport), std::end(viewport), std::begin(frustum.viewport));
  std::copy(std::begin(model), std::end(model), std::begin(frustum.model));
  std::copy(std::begin(proj), std::end(proj), std::begin(frustum.projection));
  const Viewer3DController::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);
  for (const auto &uuid : visibleSet.fixtureUuids) {
    auto bit = m_controller.m_fixtureBounds.find(uuid);
    if (bit == m_controller.m_fixtureBounds.end())
      continue;
    ScreenRect rect;
    if (!projectBounds(bit->second, rect))
      continue;
    if (intersects(rect))
      selection.push_back(uuid);
  }

  return selection;
}

std::vector<std::string> SelectionSystem::GetTrussesInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  ConfigManager &cfg = ConfigManager::Get();
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);

  ScreenRect selectionRect;
  selectionRect.minX = std::max(0, std::min(x1, x2));
  selectionRect.maxX = std::min(width, std::max(x1, x2));
  selectionRect.minY = std::max(0, std::min(y1, y2));
  selectionRect.maxY = std::min(height, std::max(y1, y2));

  auto intersects = [&](const ScreenRect &rect) {
    return !(rect.maxX < selectionRect.minX || rect.minX > selectionRect.maxX ||
             rect.maxY < selectionRect.minY || rect.minY > selectionRect.maxY);
  };

  auto projectBounds = [&](const Viewer3DController::BoundingBox &bb, ScreenRect &rect) {
    rect = ScreenRect{};
    bool visible = false;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0)
          visible = true;
      }
    }
    return visible;
  };

  std::vector<std::string> selection;
  Viewer3DController::ViewFrustumSnapshot frustum{};
  std::copy(std::begin(viewport), std::end(viewport), std::begin(frustum.viewport));
  std::copy(std::begin(model), std::end(model), std::begin(frustum.model));
  std::copy(std::begin(proj), std::end(proj), std::begin(frustum.projection));
  const Viewer3DController::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);
  for (const auto &uuid : visibleSet.trussUuids) {
    auto bit = m_controller.m_trussBounds.find(uuid);
    if (bit == m_controller.m_trussBounds.end())
      continue;
    ScreenRect rect;
    if (!projectBounds(bit->second, rect))
      continue;
    if (intersects(rect))
      selection.push_back(uuid);
  }

  return selection;
}

std::vector<std::string> SelectionSystem::GetSceneObjectsInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  ConfigManager &cfg = ConfigManager::Get();
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);

  ScreenRect selectionRect;
  selectionRect.minX = std::max(0, std::min(x1, x2));
  selectionRect.maxX = std::min(width, std::max(x1, x2));
  selectionRect.minY = std::max(0, std::min(y1, y2));
  selectionRect.maxY = std::min(height, std::max(y1, y2));

  auto intersects = [&](const ScreenRect &rect) {
    return !(rect.maxX < selectionRect.minX || rect.minX > selectionRect.maxX ||
             rect.maxY < selectionRect.minY || rect.minY > selectionRect.maxY);
  };

  auto projectBounds = [&](const Viewer3DController::BoundingBox &bb, ScreenRect &rect) {
    rect = ScreenRect{};
    bool visible = false;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0)
          visible = true;
      }
    }
    return visible;
  };

  std::vector<std::string> selection;
  Viewer3DController::ViewFrustumSnapshot frustum{};
  std::copy(std::begin(viewport), std::end(viewport), std::begin(frustum.viewport));
  std::copy(std::begin(model), std::end(model), std::begin(frustum.model));
  std::copy(std::begin(proj), std::end(proj), std::begin(frustum.projection));
  const Viewer3DController::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);
  for (const auto &uuid : visibleSet.objectUuids) {
    auto bit = m_controller.m_objectBounds.find(uuid);
    if (bit == m_controller.m_objectBounds.end())
      continue;
    ScreenRect rect;
    if (!projectBounds(bit->second, rect))
      continue;
    if (intersects(rect))
      selection.push_back(uuid);
  }

  return selection;
}
