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

#include "configmanager.h"
#include "scenedatamanager.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <unordered_set>

namespace {

struct ScreenRect {
  double minX = DBL_MAX;
  double minY = DBL_MAX;
  double maxX = -DBL_MAX;
  double maxY = -DBL_MAX;
};

struct ProjectionSnapshot {
  double model[16]{};
  double projection[16]{};
  int viewport[4]{};
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

ProjectionSnapshot CaptureProjectionSnapshot() {
  ProjectionSnapshot snapshot;
  glGetDoublev(GL_MODELVIEW_MATRIX, snapshot.model);
  glGetDoublev(GL_PROJECTION_MATRIX, snapshot.projection);
  glGetIntegerv(GL_VIEWPORT, snapshot.viewport);
  return snapshot;
}

ISelectionContext::ViewFrustumSnapshot BuildFrustumSnapshot(
    const ProjectionSnapshot &snapshot) {
  ISelectionContext::ViewFrustumSnapshot frustum{};
  std::copy(std::begin(snapshot.viewport), std::end(snapshot.viewport),
            std::begin(frustum.viewport));
  std::copy(std::begin(snapshot.model), std::end(snapshot.model),
            std::begin(frustum.model));
  std::copy(std::begin(snapshot.projection), std::end(snapshot.projection),
            std::begin(frustum.projection));
  return frustum;
}

bool ProjectBoundingBox(const ISelectionContext::BoundingBox &bb,
                       const ProjectionSnapshot &projection, int screenHeight,
                       ScreenRect &outRect, double *outMinDepth = nullptr) {
  outRect = ScreenRect{};
  bool visible = false;
  double minDepth = DBL_MAX;

  const std::array<std::array<float, 3>, 8> corners = {
      std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
      {bb.max[0], bb.min[1], bb.min[2]},
      {bb.min[0], bb.max[1], bb.min[2]},
      {bb.max[0], bb.max[1], bb.min[2]},
      {bb.min[0], bb.min[1], bb.max[2]},
      {bb.max[0], bb.min[1], bb.max[2]},
      {bb.min[0], bb.max[1], bb.max[2]},
      {bb.max[0], bb.max[1], bb.max[2]}};

  for (const auto &corner : corners) {
    double sx = 0.0;
    double sy = 0.0;
    double sz = 0.0;
    if (gluProject(corner[0], corner[1], corner[2], projection.model,
                   projection.projection, projection.viewport, &sx, &sy,
                   &sz) != GL_TRUE) {
      continue;
    }

    outRect.minX = std::min(outRect.minX, sx);
    outRect.maxX = std::max(outRect.maxX, sx);
    const double projectedY = screenHeight - sy;
    outRect.minY = std::min(outRect.minY, projectedY);
    outRect.maxY = std::max(outRect.maxY, projectedY);

    if (sz >= 0.0 && sz <= 1.0) {
      visible = true;
      minDepth = std::min(minDepth, sz);
    }
  }

  if (visible && outMinDepth)
    *outMinDepth = minDepth;

  return visible;
}

std::string FormatMeters(float mm) {
  const float meters = mm / 1000.0f;
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2f", meters);
  return std::string(buffer);
}

} // namespace

void SelectionSystem::SetHighlightUuid(const std::string &uuid) {
  m_controller.ApplyHighlightUuid(uuid);
}

void SelectionSystem::SetSelectedUuids(const std::vector<std::string> &uuids) {
  m_controller.ReplaceSelectedUuids(uuids);
}

bool SelectionSystem::GetFixtureLabelAt(int mouseX, int mouseY, int width,
                                        int height, wxString &outLabel,
                                        wxPoint &outPos,
                                        std::string *outUuid) {
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;

  const ProjectionSnapshot projection = CaptureProjectionSnapshot();
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

  const ISelectionContext::ViewFrustumSnapshot frustum =
      BuildFrustumSnapshot(projection);
  const ISelectionContext::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);

  for (const auto &uuid : visibleSet.fixtureUuids) {
    auto fixtureIt = fixtures.find(uuid);
    if (fixtureIt == fixtures.end())
      continue;
    const auto &f = fixtureIt->second;
    if (!IsLayerVisibleCached(hiddenLayers, f.layer))
      continue;
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindFixtureBounds(uuid);
    if (!bbPtr)
      continue;

    ScreenRect rect;
    double minDepth = DBL_MAX;
    if (!ProjectBoundingBox(*bbPtr, projection, height, rect, &minDepth))
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
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;

  const ProjectionSnapshot projection = CaptureProjectionSnapshot();

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  const ISelectionContext::ViewFrustumSnapshot frustum =
      BuildFrustumSnapshot(projection);
  const ISelectionContext::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);
  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  for (const auto &uuid : visibleSet.trussUuids) {
    auto trussIt = trusses.find(uuid);
    if (trussIt == trusses.end())
      continue;
    const auto &t = trussIt->second;
    if (!IsLayerVisibleCached(hiddenLayers, t.layer))
      continue;
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindTrussBounds(uuid);
    if (!bbPtr)
      continue;

    ScreenRect rect;
    double minDepth = DBL_MAX;
    if (!ProjectBoundingBox(*bbPtr, projection, height, rect, &minDepth))
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
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;

  const ProjectionSnapshot projection = CaptureProjectionSnapshot();

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const auto &objs = SceneDataManager::Instance().GetSceneObjects();
  const ISelectionContext::ViewFrustumSnapshot frustum =
      BuildFrustumSnapshot(projection);
  const ISelectionContext::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);
  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  for (const auto &uuid : visibleSet.objectUuids) {
    auto objectIt = objs.find(uuid);
    if (objectIt == objs.end())
      continue;
    const auto &o = objectIt->second;
    if (!IsLayerVisibleCached(hiddenLayers, o.layer))
      continue;
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindObjectBounds(uuid);
    if (!bbPtr)
      continue;

    ScreenRect rect;
    double minDepth = DBL_MAX;
    if (!ProjectBoundingBox(*bbPtr, projection, height, rect, &minDepth))
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
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();

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

  std::vector<std::string> selection;
  const ISelectionContext::ViewFrustumSnapshot frustum =
      BuildFrustumSnapshot(projection);
  const ISelectionContext::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);
  for (const auto &uuid : visibleSet.fixtureUuids) {
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindFixtureBounds(uuid);
    if (!bbPtr)
      continue;
    ScreenRect rect;
    if (!ProjectBoundingBox(*bbPtr, projection, height, rect))
      continue;
    if (intersects(rect))
      selection.push_back(uuid);
  }

  return selection;
}

std::vector<std::string> SelectionSystem::GetTrussesInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  ConfigManager &cfg = ConfigManager::Get();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();

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

  std::vector<std::string> selection;
  const ISelectionContext::ViewFrustumSnapshot frustum =
      BuildFrustumSnapshot(projection);
  const ISelectionContext::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);
  for (const auto &uuid : visibleSet.trussUuids) {
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindTrussBounds(uuid);
    if (!bbPtr)
      continue;
    ScreenRect rect;
    if (!ProjectBoundingBox(*bbPtr, projection, height, rect))
      continue;
    if (intersects(rect))
      selection.push_back(uuid);
  }

  return selection;
}

std::vector<std::string> SelectionSystem::GetSceneObjectsInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  ConfigManager &cfg = ConfigManager::Get();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();

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

  std::vector<std::string> selection;
  const ISelectionContext::ViewFrustumSnapshot frustum =
      BuildFrustumSnapshot(projection);
  const ISelectionContext::VisibleSet &visibleSet =
      m_controller.GetVisibleSet(frustum, hiddenLayers, true, 0.0f);
  for (const auto &uuid : visibleSet.objectUuids) {
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindObjectBounds(uuid);
    if (!bbPtr)
      continue;
    ScreenRect rect;
    if (!ProjectBoundingBox(*bbPtr, projection, height, rect))
      continue;
    if (intersects(rect))
      selection.push_back(uuid);
  }

  return selection;
}
