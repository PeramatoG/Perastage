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
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <unordered_set>
#include <wx/log.h>

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

struct Ray {
  std::array<double, 3> origin{0.0, 0.0, 0.0};
  std::array<double, 3> direction{0.0, 0.0, 1.0};
};

bool ProjectBoundingBox(const ISelectionContext::BoundingBox &bb,
                        const ProjectionSnapshot &projection, int screenHeight,
                        ScreenRect &outRect, double *outMinDepth);

constexpr int kHoverGridCellSizePx = 96;

void InvalidateHoverGrid(SelectionSystem::QueryCache::HoverGridIndex &index) {
  index.valid = false;
  index.sourceVisibleSet = nullptr;
  index.sourceCount = 0;
  index.viewportWidth = 0;
  index.viewportHeight = 0;
  index.cells.clear();
  index.candidates.clear();
}

long long MakeHoverGridKey(int cellX, int cellY) {
  return (static_cast<long long>(cellX) << 32) ^
         static_cast<unsigned int>(cellY);
}

std::vector<int> QueryHoverGridCandidates(
    const SelectionSystem::QueryCache::HoverGridIndex &index, int mouseX,
    int mouseY) {
  if (!index.valid || index.viewportWidth <= 0 || index.viewportHeight <= 0)
    return {};
  if (mouseX < 0 || mouseY < 0 || mouseX >= index.viewportWidth ||
      mouseY >= index.viewportHeight) {
    return {};
  }

  const int baseCellX = mouseX / kHoverGridCellSizePx;
  const int baseCellY = mouseY / kHoverGridCellSizePx;
  std::vector<int> result;
  result.reserve(9);
  for (int offsetY = -1; offsetY <= 1; ++offsetY) {
    for (int offsetX = -1; offsetX <= 1; ++offsetX) {
      const int cellX = baseCellX + offsetX;
      const int cellY = baseCellY + offsetY;
      if (cellX < 0 || cellY < 0)
        continue;
      const auto cellIt = index.cells.find(MakeHoverGridKey(cellX, cellY));
      if (cellIt == index.cells.end() || cellIt->second.candidateIndex < 0)
        continue;
      const int candidateIndex = cellIt->second.candidateIndex;
      if (std::find(result.begin(), result.end(), candidateIndex) == result.end())
        result.push_back(candidateIndex);
    }
  }
  return result;
}

void BuildHoverGridIndex(
    const std::vector<std::string> &uuids, const ProjectionSnapshot &projection,
    int screenWidth, int screenHeight,
    const std::function<const ISelectionContext::BoundingBox *(
        const std::string &)> &findBounds,
    SelectionSystem::QueryCache::HoverGridIndex &index) {
  InvalidateHoverGrid(index);
  if (screenWidth <= 0 || screenHeight <= 0 || uuids.empty())
    return;

  index.viewportWidth = screenWidth;
  index.viewportHeight = screenHeight;
  index.candidates.reserve(uuids.size());

  for (const auto &uuid : uuids) {
    const ISelectionContext::BoundingBox *bbPtr = findBounds(uuid);
    if (!bbPtr)
      continue;

    ScreenRect rect;
    double minDepth = 1.0;
    if (!ProjectBoundingBox(*bbPtr, projection, screenHeight, rect, &minDepth))
      continue;

    rect.minX = std::clamp(rect.minX, 0.0, static_cast<double>(screenWidth - 1));
    rect.maxX = std::clamp(rect.maxX, 0.0, static_cast<double>(screenWidth - 1));
    rect.minY = std::clamp(rect.minY, 0.0, static_cast<double>(screenHeight - 1));
    rect.maxY = std::clamp(rect.maxY, 0.0, static_cast<double>(screenHeight - 1));
    if (rect.minX > rect.maxX || rect.minY > rect.maxY)
      continue;

    SelectionSystem::QueryCache::HoverCandidate candidate;
    candidate.uuid = uuid;
    candidate.minX = rect.minX;
    candidate.minY = rect.minY;
    candidate.maxX = rect.maxX;
    candidate.maxY = rect.maxY;
    const int candidateIndex = static_cast<int>(index.candidates.size());
    index.candidates.push_back(std::move(candidate));

    const int minCellX = static_cast<int>(rect.minX) / kHoverGridCellSizePx;
    const int maxCellX = static_cast<int>(rect.maxX) / kHoverGridCellSizePx;
    const int minCellY = static_cast<int>(rect.minY) / kHoverGridCellSizePx;
    const int maxCellY = static_cast<int>(rect.maxY) / kHoverGridCellSizePx;
    for (int cellY = minCellY; cellY <= maxCellY; ++cellY) {
      for (int cellX = minCellX; cellX <= maxCellX; ++cellX) {
        const long long key = MakeHoverGridKey(cellX, cellY);
        auto &segCell = index.cells[key];
        if (segCell.candidateIndex < 0 || minDepth <= segCell.minDepth) {
          segCell.candidateIndex = candidateIndex;
          segCell.minDepth = minDepth;
        }
      }
    }
  }

  index.valid = true;
}

std::unordered_set<std::string> SnapshotHiddenLayers(const ConfigManager &cfg) {
  return cfg.GetHiddenLayers();
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

bool BuildMouseRay(int mouseX, int mouseY, int screenHeight,
                   const ProjectionSnapshot &projection, Ray &outRay) {
  const double winX = static_cast<double>(mouseX);
  const double winY = static_cast<double>(screenHeight - mouseY);

  double nearX = 0.0;
  double nearY = 0.0;
  double nearZ = 0.0;
  double farX = 0.0;
  double farY = 0.0;
  double farZ = 0.0;

  if (gluUnProject(winX, winY, 0.0, projection.model, projection.projection,
                   projection.viewport, &nearX, &nearY, &nearZ) != GL_TRUE) {
    return false;
  }
  if (gluUnProject(winX, winY, 1.0, projection.model, projection.projection,
                   projection.viewport, &farX, &farY, &farZ) != GL_TRUE) {
    return false;
  }

  const double dirX = farX - nearX;
  const double dirY = farY - nearY;
  const double dirZ = farZ - nearZ;
  const double dirLen = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
  if (dirLen <= 1e-12)
    return false;

  outRay.origin = {nearX, nearY, nearZ};
  outRay.direction = {dirX / dirLen, dirY / dirLen, dirZ / dirLen};
  return true;
}

bool ReadWorldPointFromDepth(int mouseX, int mouseY, int screenHeight,
                             const ProjectionSnapshot &projection,
                             std::array<double, 3> &outWorldPoint) {
  const int sampleY = screenHeight - mouseY;
  float depth = 1.0f;
  glReadPixels(mouseX, sampleY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
  if (depth >= 1.0f)
    return false;

  const double winX = static_cast<double>(mouseX);
  const double winY = static_cast<double>(sampleY);
  double worldX = 0.0;
  double worldY = 0.0;
  double worldZ = 0.0;
  if (gluUnProject(winX, winY, static_cast<double>(depth), projection.model,
                   projection.projection, projection.viewport, &worldX,
                   &worldY, &worldZ) != GL_TRUE) {
    return false;
  }

  outWorldPoint = {worldX, worldY, worldZ};
  return true;
}

bool PointInAabb(const std::array<double, 3> &point,
                 const ISelectionContext::BoundingBox &bb,
                 double epsilon = 1e-4) {
  return point[0] >= static_cast<double>(bb.min[0]) - epsilon &&
         point[0] <= static_cast<double>(bb.max[0]) + epsilon &&
         point[1] >= static_cast<double>(bb.min[1]) - epsilon &&
         point[1] <= static_cast<double>(bb.max[1]) + epsilon &&
         point[2] >= static_cast<double>(bb.min[2]) - epsilon &&
         point[2] <= static_cast<double>(bb.max[2]) + epsilon;
}

double DistanceSquaredToAabbCenter(const std::array<double, 3> &point,
                                   const ISelectionContext::BoundingBox &bb) {
  const double cx = 0.5 * static_cast<double>(bb.min[0] + bb.max[0]);
  const double cy = 0.5 * static_cast<double>(bb.min[1] + bb.max[1]);
  const double cz = 0.5 * static_cast<double>(bb.min[2] + bb.max[2]);
  const double dx = point[0] - cx;
  const double dy = point[1] - cy;
  const double dz = point[2] - cz;
  return dx * dx + dy * dy + dz * dz;
}

bool IntersectRayWithAabb(const Ray &ray, const ISelectionContext::BoundingBox &bb,
                          double &outHitDistance) {
  constexpr double kEpsilon = 1e-9;
  double tMin = 0.0;
  double tMax = DBL_MAX;

  for (int axis = 0; axis < 3; ++axis) {
    const double origin = ray.origin[axis];
    const double direction = ray.direction[axis];
    const double minBound = bb.min[axis];
    const double maxBound = bb.max[axis];

    if (std::abs(direction) <= kEpsilon) {
      if (origin < minBound || origin > maxBound)
        return false;
      continue;
    }

    const double invDirection = 1.0 / direction;
    double t1 = (minBound - origin) * invDirection;
    double t2 = (maxBound - origin) * invDirection;
    if (t1 > t2)
      std::swap(t1, t2);

    tMin = std::max(tMin, t1);
    tMax = std::min(tMax, t2);
    if (tMin > tMax)
      return false;
  }

  outHitDistance = tMin;
  return true;
}

bool ProjectBoundingBoxCenter(const ISelectionContext::BoundingBox &bb,
                              const ProjectionSnapshot &projection,
                              int screenHeight, wxPoint &outPos) {
  const double centerX = 0.5 * static_cast<double>(bb.min[0] + bb.max[0]);
  const double centerY = 0.5 * static_cast<double>(bb.min[1] + bb.max[1]);
  const double centerZ = 0.5 * static_cast<double>(bb.min[2] + bb.max[2]);

  double sx = 0.0;
  double sy = 0.0;
  double sz = 0.0;
  if (gluProject(centerX, centerY, centerZ, projection.model,
                 projection.projection, projection.viewport, &sx, &sy,
                 &sz) != GL_TRUE) {
    return false;
  }

  outPos.x = static_cast<int>(sx);
  outPos.y = static_cast<int>(screenHeight - sy);
  return true;
}

std::string FormatMeters(float mm) {
  const float meters = mm / 1000.0f;
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2f", meters);
  return std::string(buffer);
}

bool ProjectionMatchesCache(const ProjectionSnapshot &projection,
                            const SelectionSystem::QueryCache &cache) {
  if (!cache.valid)
    return false;
  if (!std::equal(std::begin(projection.viewport), std::end(projection.viewport),
                  std::begin(cache.viewport))) {
    return false;
  }
  if (!std::equal(std::begin(projection.model), std::end(projection.model),
                  cache.model.begin())) {
    return false;
  }
  return std::equal(std::begin(projection.projection),
                    std::end(projection.projection), cache.projection.begin());
}

void CacheProjectionSnapshot(const ProjectionSnapshot &projection,
                             SelectionSystem::QueryCache &cache) {
  cache.valid = true;
  std::copy(std::begin(projection.viewport), std::end(projection.viewport),
            std::begin(cache.viewport));
  std::copy(std::begin(projection.model), std::end(projection.model),
            cache.model.begin());
  std::copy(std::begin(projection.projection), std::end(projection.projection),
            cache.projection.begin());
  cache.frustum = BuildFrustumSnapshot(projection);
  cache.visibleSet = nullptr;
  InvalidateHoverGrid(cache.fixtureHoverIndex);
  InvalidateHoverGrid(cache.trussHoverIndex);
  InvalidateHoverGrid(cache.objectHoverIndex);
}

void LogQueryMetrics(const char *label, const SelectionSystem::QueryMetrics &metrics,
                     int queryCounter) {
#ifndef NDEBUG
  if (queryCounter % 60 == 0) {
    wxLogDebug(
        "SelectionSystem metrics [%s] projection=%lldus depth=%lldus candidate=%lldus",
        label, static_cast<long long>(metrics.projection.count()),
        static_cast<long long>(metrics.depthRead.count()),
        static_cast<long long>(metrics.candidateLoop.count()));
  }
#else
  (void)label;
  (void)metrics;
  (void)queryCounter;
#endif
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
                                        std::string *outUuid,
                                        bool confirmDepth) {
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;

  QueryMetrics metrics;
  const auto projectionStart = std::chrono::steady_clock::now();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();
  const bool projectionChanged = !ProjectionMatchesCache(projection, m_queryCache);
  if (projectionChanged) {
    CacheProjectionSnapshot(projection, m_queryCache);
    m_queryCache.hiddenLayers.clear();
  }
  metrics.projection = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - projectionStart);

  Ray mouseRay;
  if (!BuildMouseRay(mouseX, mouseY, height, projection, mouseRay))
    return false;
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  if (projectionChanged || hiddenLayers != m_queryCache.hiddenLayers) {
    m_queryCache.hiddenLayers = hiddenLayers;
    m_queryCache.visibleSet = nullptr;
  }
  const auto depthReadStart = std::chrono::steady_clock::now();
  bool hasDepthWorldPoint = false;
  std::array<double, 3> depthWorldPoint{};
  const bool shouldReadDepth = confirmDepth &&
                               (projectionChanged || m_queryCache.depthMouseX != mouseX ||
                                m_queryCache.depthMouseY != mouseY ||
                                m_queryCache.depthHeight != height);
  if (shouldReadDepth) {
    m_queryCache.hasDepthWorldPoint =
        ReadWorldPointFromDepth(mouseX, mouseY, height, projection,
                                m_queryCache.depthWorldPoint);
    m_queryCache.depthMouseX = mouseX;
    m_queryCache.depthMouseY = mouseY;
    m_queryCache.depthHeight = height;
  }
  if (confirmDepth) {
    hasDepthWorldPoint = m_queryCache.hasDepthWorldPoint;
    depthWorldPoint = m_queryCache.depthWorldPoint;
  }
  metrics.depthRead = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - depthReadStart);
  bool showName = cfg.GetFloat("label_show_name") != 0.0f;
  bool showId = cfg.GetFloat("label_show_id") != 0.0f;
  bool showDmx = cfg.GetFloat("label_show_dmx") != 0.0f;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();

  bool found = false;
  double bestHitDistance = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;

  if (!m_queryCache.visibleSet) {
    m_queryCache.visibleSet =
        &m_controller.GetVisibleSet(m_queryCache.frustum, m_queryCache.hiddenLayers,
                                    true, 0.0f);
  }
  const ISelectionContext::VisibleSet &visibleSet = *m_queryCache.visibleSet;
  if (!m_queryCache.fixtureHoverIndex.valid ||
      m_queryCache.fixtureHoverIndex.sourceVisibleSet != &visibleSet ||
      m_queryCache.fixtureHoverIndex.sourceCount != visibleSet.fixtureUuids.size() ||
      m_queryCache.fixtureHoverIndex.viewportWidth != width ||
      m_queryCache.fixtureHoverIndex.viewportHeight != height) {
    BuildHoverGridIndex(
        visibleSet.fixtureUuids, projection, width, height,
        [this](const std::string &uuid) { return m_controller.FindFixtureBounds(uuid); },
        m_queryCache.fixtureHoverIndex);
    m_queryCache.fixtureHoverIndex.sourceVisibleSet = &visibleSet;
    m_queryCache.fixtureHoverIndex.sourceCount = visibleSet.fixtureUuids.size();
  }
  const std::vector<int> candidateIndices =
      QueryHoverGridCandidates(m_queryCache.fixtureHoverIndex, mouseX, mouseY);

  const auto candidateLoopStart = std::chrono::steady_clock::now();
  for (const int candidateIndex : candidateIndices) {
    if (candidateIndex < 0 ||
        candidateIndex >=
            static_cast<int>(m_queryCache.fixtureHoverIndex.candidates.size())) {
      continue;
    }
    const auto &candidate = m_queryCache.fixtureHoverIndex.candidates[candidateIndex];
    if (mouseX < candidate.minX || mouseX > candidate.maxX || mouseY < candidate.minY ||
        mouseY > candidate.maxY)
      continue;

    auto fixtureIt = fixtures.find(candidate.uuid);
    if (fixtureIt == fixtures.end())
      continue;
    const auto &f = fixtureIt->second;
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindFixtureBounds(candidate.uuid);
    if (!bbPtr)
      continue;

    const bool depthContainsPoint =
        hasDepthWorldPoint && PointInAabb(depthWorldPoint, *bbPtr);
    if (hasDepthWorldPoint && !depthContainsPoint)
      continue;

    double hitDistance = DBL_MAX;
    if (!IntersectRayWithAabb(mouseRay, *bbPtr, hitDistance))
      continue;

    const double candidateScore = hasDepthWorldPoint
                                      ? DistanceSquaredToAabbCenter(depthWorldPoint,
                                                                    *bbPtr)
                                      : hitDistance;

    if (candidateScore < bestHitDistance) {
      wxString label;
      if (showName)
        label = f.instanceName.empty() ? wxString::FromUTF8(candidate.uuid)
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

      wxPoint projectedCenter;
      if (!ProjectBoundingBoxCenter(*bbPtr, projection, height, projectedCenter))
        continue;

      bestHitDistance = candidateScore;
      bestPos = projectedCenter;
      bestLabel = label;
      bestUuid = candidate.uuid;
      found = true;
    }
  }
  metrics.candidateLoop = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - candidateLoopStart);
  LogQueryMetrics("fixture", metrics, ++m_queryCounter);

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
                                      std::string *outUuid,
                                      bool confirmDepth) {
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;

  QueryMetrics metrics;
  const auto projectionStart = std::chrono::steady_clock::now();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();
  const bool projectionChanged = !ProjectionMatchesCache(projection, m_queryCache);
  if (projectionChanged) {
    CacheProjectionSnapshot(projection, m_queryCache);
    m_queryCache.hiddenLayers.clear();
  }
  metrics.projection = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - projectionStart);
  Ray mouseRay;
  if (!BuildMouseRay(mouseX, mouseY, height, projection, mouseRay))
    return false;
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  if (projectionChanged || hiddenLayers != m_queryCache.hiddenLayers) {
    m_queryCache.hiddenLayers = hiddenLayers;
    m_queryCache.visibleSet = nullptr;
  }
  const auto depthReadStart = std::chrono::steady_clock::now();
  bool hasDepthWorldPoint = false;
  std::array<double, 3> depthWorldPoint{};
  const bool shouldReadDepth = confirmDepth &&
                               (projectionChanged || m_queryCache.depthMouseX != mouseX ||
                                m_queryCache.depthMouseY != mouseY ||
                                m_queryCache.depthHeight != height);
  if (shouldReadDepth) {
    m_queryCache.hasDepthWorldPoint =
        ReadWorldPointFromDepth(mouseX, mouseY, height, projection,
                                m_queryCache.depthWorldPoint);
    m_queryCache.depthMouseX = mouseX;
    m_queryCache.depthMouseY = mouseY;
    m_queryCache.depthHeight = height;
  }
  if (confirmDepth) {
    hasDepthWorldPoint = m_queryCache.hasDepthWorldPoint;
    depthWorldPoint = m_queryCache.depthWorldPoint;
  }
  metrics.depthRead = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - depthReadStart);
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  if (!m_queryCache.visibleSet) {
    m_queryCache.visibleSet =
        &m_controller.GetVisibleSet(m_queryCache.frustum, m_queryCache.hiddenLayers,
                                    true, 0.0f);
  }
  const ISelectionContext::VisibleSet &visibleSet = *m_queryCache.visibleSet;
  if (!m_queryCache.trussHoverIndex.valid ||
      m_queryCache.trussHoverIndex.sourceVisibleSet != &visibleSet ||
      m_queryCache.trussHoverIndex.sourceCount != visibleSet.trussUuids.size() ||
      m_queryCache.trussHoverIndex.viewportWidth != width ||
      m_queryCache.trussHoverIndex.viewportHeight != height) {
    BuildHoverGridIndex(
        visibleSet.trussUuids, projection, width, height,
        [this](const std::string &uuid) { return m_controller.FindTrussBounds(uuid); },
        m_queryCache.trussHoverIndex);
    m_queryCache.trussHoverIndex.sourceVisibleSet = &visibleSet;
    m_queryCache.trussHoverIndex.sourceCount = visibleSet.trussUuids.size();
  }
  const std::vector<int> candidateIndices =
      QueryHoverGridCandidates(m_queryCache.trussHoverIndex, mouseX, mouseY);
  bool found = false;
  double bestHitDistance = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  const auto candidateLoopStart = std::chrono::steady_clock::now();
  for (const int candidateIndex : candidateIndices) {
    if (candidateIndex < 0 ||
        candidateIndex >=
            static_cast<int>(m_queryCache.trussHoverIndex.candidates.size())) {
      continue;
    }
    const auto &candidate = m_queryCache.trussHoverIndex.candidates[candidateIndex];
    if (mouseX < candidate.minX || mouseX > candidate.maxX || mouseY < candidate.minY ||
        mouseY > candidate.maxY)
      continue;

    auto trussIt = trusses.find(candidate.uuid);
    if (trussIt == trusses.end())
      continue;
    const auto &t = trussIt->second;
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindTrussBounds(candidate.uuid);
    if (!bbPtr)
      continue;

    const bool depthContainsPoint =
        hasDepthWorldPoint && PointInAabb(depthWorldPoint, *bbPtr);
    if (hasDepthWorldPoint && !depthContainsPoint)
      continue;

    double hitDistance = DBL_MAX;
    if (!IntersectRayWithAabb(mouseRay, *bbPtr, hitDistance))
      continue;

    const double candidateScore = hasDepthWorldPoint
                                      ? DistanceSquaredToAabbCenter(depthWorldPoint,
                                                                    *bbPtr)
                                      : hitDistance;

    if (candidateScore < bestHitDistance) {
      wxPoint projectedCenter;
      if (!ProjectBoundingBoxCenter(*bbPtr, projection, height, projectedCenter))
        continue;

      bestHitDistance = candidateScore;
      bestPos = projectedCenter;
      bestLabel = t.name.empty() ? wxString::FromUTF8(candidate.uuid)
                                 : wxString::FromUTF8(t.name);
      float baseHeight = t.transform.o[2] - t.heightMm * 0.5f;
      std::string hStr = FormatMeters(baseHeight);
      bestLabel += wxString::Format("\nh = %s m", hStr.c_str());
      bestUuid = candidate.uuid;
      found = true;
    }
  }
  metrics.candidateLoop = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - candidateLoopStart);
  LogQueryMetrics("truss", metrics, ++m_queryCounter);

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
                                            std::string *outUuid,
                                            bool confirmDepth) {
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;

  QueryMetrics metrics;
  const auto projectionStart = std::chrono::steady_clock::now();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();
  const bool projectionChanged = !ProjectionMatchesCache(projection, m_queryCache);
  if (projectionChanged) {
    CacheProjectionSnapshot(projection, m_queryCache);
    m_queryCache.hiddenLayers.clear();
  }
  metrics.projection = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - projectionStart);
  Ray mouseRay;
  if (!BuildMouseRay(mouseX, mouseY, height, projection, mouseRay))
    return false;
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  if (projectionChanged || hiddenLayers != m_queryCache.hiddenLayers) {
    m_queryCache.hiddenLayers = hiddenLayers;
    m_queryCache.visibleSet = nullptr;
  }
  const auto depthReadStart = std::chrono::steady_clock::now();
  bool hasDepthWorldPoint = false;
  std::array<double, 3> depthWorldPoint{};
  const bool shouldReadDepth = confirmDepth &&
                               (projectionChanged || m_queryCache.depthMouseX != mouseX ||
                                m_queryCache.depthMouseY != mouseY ||
                                m_queryCache.depthHeight != height);
  if (shouldReadDepth) {
    m_queryCache.hasDepthWorldPoint =
        ReadWorldPointFromDepth(mouseX, mouseY, height, projection,
                                m_queryCache.depthWorldPoint);
    m_queryCache.depthMouseX = mouseX;
    m_queryCache.depthMouseY = mouseY;
    m_queryCache.depthHeight = height;
  }
  if (confirmDepth) {
    hasDepthWorldPoint = m_queryCache.hasDepthWorldPoint;
    depthWorldPoint = m_queryCache.depthWorldPoint;
  }
  metrics.depthRead = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - depthReadStart);
  const auto &objs = SceneDataManager::Instance().GetSceneObjects();
  if (!m_queryCache.visibleSet) {
    m_queryCache.visibleSet =
        &m_controller.GetVisibleSet(m_queryCache.frustum, m_queryCache.hiddenLayers,
                                    true, 0.0f);
  }
  const ISelectionContext::VisibleSet &visibleSet = *m_queryCache.visibleSet;
  if (!m_queryCache.objectHoverIndex.valid ||
      m_queryCache.objectHoverIndex.sourceVisibleSet != &visibleSet ||
      m_queryCache.objectHoverIndex.sourceCount != visibleSet.objectUuids.size() ||
      m_queryCache.objectHoverIndex.viewportWidth != width ||
      m_queryCache.objectHoverIndex.viewportHeight != height) {
    BuildHoverGridIndex(
        visibleSet.objectUuids, projection, width, height,
        [this](const std::string &uuid) { return m_controller.FindObjectBounds(uuid); },
        m_queryCache.objectHoverIndex);
    m_queryCache.objectHoverIndex.sourceVisibleSet = &visibleSet;
    m_queryCache.objectHoverIndex.sourceCount = visibleSet.objectUuids.size();
  }
  const std::vector<int> candidateIndices =
      QueryHoverGridCandidates(m_queryCache.objectHoverIndex, mouseX, mouseY);
  bool found = false;
  double bestHitDistance = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  const auto candidateLoopStart = std::chrono::steady_clock::now();
  for (const int candidateIndex : candidateIndices) {
    if (candidateIndex < 0 ||
        candidateIndex >=
            static_cast<int>(m_queryCache.objectHoverIndex.candidates.size())) {
      continue;
    }
    const auto &candidate = m_queryCache.objectHoverIndex.candidates[candidateIndex];
    if (mouseX < candidate.minX || mouseX > candidate.maxX || mouseY < candidate.minY ||
        mouseY > candidate.maxY)
      continue;

    auto objectIt = objs.find(candidate.uuid);
    if (objectIt == objs.end())
      continue;
    const auto &o = objectIt->second;
    const ISelectionContext::BoundingBox *bbPtr =
        m_controller.FindObjectBounds(candidate.uuid);
    if (!bbPtr)
      continue;

    const bool depthContainsPoint =
        hasDepthWorldPoint && PointInAabb(depthWorldPoint, *bbPtr);
    if (hasDepthWorldPoint && !depthContainsPoint)
      continue;

    double hitDistance = DBL_MAX;
    if (!IntersectRayWithAabb(mouseRay, *bbPtr, hitDistance))
      continue;

    const double candidateScore = hasDepthWorldPoint
                                      ? DistanceSquaredToAabbCenter(depthWorldPoint,
                                                                    *bbPtr)
                                      : hitDistance;

    if (candidateScore < bestHitDistance) {
      wxPoint projectedCenter;
      if (!ProjectBoundingBoxCenter(*bbPtr, projection, height, projectedCenter))
        continue;

      bestHitDistance = candidateScore;
      bestPos = projectedCenter;
      bestLabel = o.name.empty() ? wxString::FromUTF8(candidate.uuid)
                                 : wxString::FromUTF8(o.name);
      bestUuid = candidate.uuid;
      found = true;
    }
  }
  metrics.candidateLoop = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - candidateLoopStart);
  LogQueryMetrics("object", metrics, ++m_queryCounter);

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
