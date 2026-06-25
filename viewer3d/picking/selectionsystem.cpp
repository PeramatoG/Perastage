#include "selectionsystem.h"

#include "picking_coordinate_utils.h"
#include "render/opengl_state_guard.h"

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
#include "units/unit_label_utils.h"
#include "units/units.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
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

bool IsIdBufferPickingEnabled(const ConfigManager &cfg) {
  return cfg.GetFloat("viewer3d_pick_use_id_buffer") >= 0.5f;
}

bool IsAmbiguousDepthConfirmEnabled(const ConfigManager &cfg) {
  return cfg.GetFloat("viewer3d_pick_confirm_depth_ambiguous") >= 0.5f;
}

// Returns whether a GL renderer string appears to be an Intel Windows driver.
bool IsWindowsIntelRenderer() {
#ifdef _WIN32
  const char *vendor = viewer3d::render::SafeGlString(GL_VENDOR);
  const char *renderer = viewer3d::render::SafeGlString(GL_RENDERER);
  const std::string vendorText = vendor ? vendor : "";
  const std::string rendererText = renderer ? renderer : "";
  auto containsIntel = [](std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text.find("intel") != std::string::npos;
  };
  return containsIntel(vendorText) || containsIntel(rendererText);
#else
  return false;
#endif
}

// Stores the session-level depth-read picking disable state.
bool &DepthReadPickingDisabledForSession() {
  static bool disabledForSession = false;
  return disabledForSession;
}

// Returns whether depth-buffer picking may use glReadPixels for this session.
bool IsDepthReadPickingAllowed(const ConfigManager &cfg, const char **outReason) {
  if (!IsAmbiguousDepthConfirmEnabled(cfg)) {
    if (outReason)
      *outReason = "viewer3d_pick_confirm_depth_ambiguous is disabled";
    return false;
  }
  if (DepthReadPickingDisabledForSession()) {
    if (outReason)
      *outReason = "depth-read picking was disabled after an earlier GL error";
    return false;
  }
  if (IsWindowsIntelRenderer()) {
    if (outReason)
      *outReason = "depth-read picking is disabled on Windows Intel OpenGL drivers";
    return false;
  }
  if (outReason)
    *outReason = "enabled";
  return true;
}

// Disables depth-buffer picking for the remainder of the session.
void DisableDepthReadPickingForSession() {
  DepthReadPickingDisabledForSession() = true;
}

// Returns whether this query should perform optional depth confirmation.
bool ShouldConfirmDepthForQuery(bool confirmDepth, const ConfigManager &cfg) {
  if (!confirmDepth)
    return false;
  const char *reason = nullptr;
  const bool allowed = IsDepthReadPickingAllowed(cfg, &reason);
  if (!allowed)
    wxLogDebug("Picking: skipped depth confirmation because %s.",
               reason ? reason : "depth-read picking is not allowed");
  return allowed;
}

// Captures the current OpenGL projection matrices and viewport.
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

// Builds a world-space ray from validated mouse coordinates.
bool BuildMouseRay(int mouseX, int mouseY, int screenHeight,
                   const ProjectionSnapshot &projection, Ray &outRay) {
  int framebufferX = 0;
  int framebufferY = 0;
  if (!TryConvertMouseToFramebufferPoint(mouseX, mouseY, projection.viewport[2],
                                         screenHeight, framebufferX,
                                         framebufferY)) {
    return false;
  }
  const double winX = static_cast<double>(framebufferX);
  const double winY = static_cast<double>(framebufferY);

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

// Reads and unprojects the depth value under a validated mouse coordinate.
bool ReadWorldPointFromDepth(int mouseX, int mouseY, int screenHeight,
                             const ProjectionSnapshot &projection,
                             std::array<double, 3> &outWorldPoint) {
  if (!viewer3d::render::HasCurrentOpenGLContext()) {
    wxLogDebug("Picking: skipped due to invalid context before depth read.");
    DisableDepthReadPickingForSession();
    return false;
  }

  int currentViewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, currentViewport);
  if (currentViewport[2] <= 0 || currentViewport[3] <= 0 ||
      projection.viewport[2] <= 0 || projection.viewport[3] <= 0) {
    wxLogDebug(
        "Picking: skipped due to invalid viewport before depth read viewport=(%d,%d,%d,%d).",
        currentViewport[0], currentViewport[1], currentViewport[2],
        currentViewport[3]);
    DisableDepthReadPickingForSession();
    return false;
  }

  int framebufferX = 0;
  int framebufferY = 0;
  if (!TryConvertMouseToFramebufferPoint(mouseX, mouseY, projection.viewport[2],
                                         screenHeight, framebufferX,
                                         framebufferY)) {
    return false;
  }

  if (framebufferX < currentViewport[0] || framebufferY < currentViewport[1] ||
      framebufferX >= currentViewport[0] + currentViewport[2] ||
      framebufferY >= currentViewport[1] + currentViewport[3]) {
    wxLogDebug(
        "Picking: skipped due to out-of-viewport depth read mouse=(%d,%d) framebuffer=(%d,%d) viewport=(%d,%d,%d,%d).",
        mouseX, mouseY, framebufferX, framebufferY, currentViewport[0],
        currentViewport[1], currentViewport[2], currentViewport[3]);
    return false;
  }

  GLint framebuffer = 0;
  GLint readFramebuffer = 0;
  GLint readBuffer = GL_BACK;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  if (GLEW_VERSION_3_0 || GLEW_EXT_framebuffer_blit)
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
  else
    readFramebuffer = framebuffer;
  glGetIntegerv(GL_READ_BUFFER, &readBuffer);
  if (readFramebuffer == 0) {
    wxLogDebug(
        "Picking: skipped depth read because no controlled read framebuffer is bound.");
    DisableDepthReadPickingForSession();
    return false;
  }
  if (readBuffer == GL_NONE) {
    wxLogDebug(
        "Picking: skipped depth read because the active read buffer is GL_NONE framebuffer=%d readFramebuffer=%d.",
        framebuffer, readFramebuffer);
    DisableDepthReadPickingForSession();
    return false;
  }

  if (GLEW_VERSION_3_0 || GLEW_EXT_framebuffer_object ||
      GLEW_ARB_framebuffer_object) {
    const GLenum framebufferStatus = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
      wxLogDebug(
          "Picking: skipped depth read because read framebuffer is incomplete status=0x%04x framebuffer=%d readFramebuffer=%d.",
          static_cast<unsigned int>(framebufferStatus), framebuffer,
          readFramebuffer);
      DisableDepthReadPickingForSession();
      return false;
    }
  }

  viewer3d::render::ClearOpenGLErrors();
  float depth = 1.0f;
  glReadPixels(framebufferX, framebufferY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT,
               &depth);
  const GLenum readError = viewer3d::render::DrainOpenGLErrors();
  if (readError != GL_NO_ERROR) {
    wxLogDebug(
        "Picking: glReadPixels depth read failed error=0x%04x framebuffer=%d readFramebuffer=%d readBuffer=0x%04x viewport=(%d,%d,%d,%d) vendor=%s renderer=%s. Depth-read picking is disabled for this session.",
        static_cast<unsigned int>(readError), framebuffer, readFramebuffer,
        static_cast<unsigned int>(readBuffer), currentViewport[0],
        currentViewport[1], currentViewport[2], currentViewport[3],
        viewer3d::render::SafeGlString(GL_VENDOR),
        viewer3d::render::SafeGlString(GL_RENDERER));
    DisableDepthReadPickingForSession();
    return false;
  }
  if (depth < 0.0f || depth >= 1.0f || !std::isfinite(depth))
    return false;

  const double winX = static_cast<double>(framebufferX);
  const double winY = static_cast<double>(framebufferY);
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

// Formats a distance for hover labels using the selected project units.
std::string FormatDistanceLabel(float mm, const ConfigManager &cfg) {
  const auto distanceUnit =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  return Units::DistanceValueWithUnit(mm, distanceUnit,
                                      Units::ValueFormatContext::Label);
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
}

bool IsUuidValidForTarget(const std::string &uuid,
                          SelectionSystem::HoverPickTarget target,
                          const std::unordered_set<std::string> &hiddenLayers,
                          const ISelectionContext &controller,
                          const ISelectionContext::BoundingBox **outBounds) {
  switch (target) {
  case SelectionSystem::HoverPickTarget::Fixture: {
    const auto &fixtures = SceneDataManager::Instance().GetFixtures();
    auto fixtureIt = fixtures.find(uuid);
    if (fixtureIt == fixtures.end() ||
        !IsLayerVisibleCached(hiddenLayers, fixtureIt->second.layer)) {
      return false;
    }
    const ISelectionContext::BoundingBox *bbPtr =
        controller.FindFixtureBounds(uuid);
    if (!bbPtr)
      return false;
    if (outBounds)
      *outBounds = bbPtr;
    return true;
  }
  case SelectionSystem::HoverPickTarget::Truss: {
    const auto &trusses = SceneDataManager::Instance().GetTrusses();
    auto trussIt = trusses.find(uuid);
    if (trussIt == trusses.end() ||
        !IsLayerVisibleCached(hiddenLayers, trussIt->second.layer)) {
      return false;
    }
    const ISelectionContext::BoundingBox *bbPtr = controller.FindTrussBounds(uuid);
    if (!bbPtr)
      return false;
    if (outBounds)
      *outBounds = bbPtr;
    return true;
  }
  case SelectionSystem::HoverPickTarget::SceneObject: {
    const auto &objects = SceneDataManager::Instance().GetSceneObjects();
    auto objectIt = objects.find(uuid);
    if (objectIt == objects.end() ||
        !IsLayerVisibleCached(hiddenLayers, objectIt->second.layer)) {
      return false;
    }
    const ISelectionContext::BoundingBox *bbPtr = controller.FindObjectBounds(uuid);
    if (!bbPtr)
      return false;
    if (outBounds)
      *outBounds = bbPtr;
    return true;
  }
  }
  return false;
}

const std::vector<std::string> &VisibleUuidsForTarget(
    const ISelectionContext::VisibleSet &visibleSet,
    SelectionSystem::HoverPickTarget target) {
  switch (target) {
  case SelectionSystem::HoverPickTarget::Fixture:
    return visibleSet.fixtureUuids;
  case SelectionSystem::HoverPickTarget::Truss:
    return visibleSet.trussUuids;
  case SelectionSystem::HoverPickTarget::SceneObject:
    return visibleSet.objectUuids;
  }
  return visibleSet.fixtureUuids;
}

void LogQueryMetrics(const char *label, const SelectionSystem::QueryMetrics &metrics,
                     const SelectionSystem::QueryTelemetry &telemetry,
                     int queryCounter) {
#ifndef NDEBUG
  if (queryCounter % 60 == 0) {
    wxLogDebug(
        "SelectionSystem metrics [%s] total=%lldus projection=%lldus depth=%lldus candidate=%lldus idPath=%d fallback=%d reproj=%d repaints=%d",
        label, static_cast<long long>(metrics.total.count()),
        static_cast<long long>(metrics.projection.count()),
        static_cast<long long>(metrics.depthRead.count()),
        static_cast<long long>(metrics.candidateLoop.count()),
        telemetry.idPathQueries, telemetry.fallbackQueries,
        telemetry.reprojections, telemetry.repaintEstimates);
  }
#else
  (void)label;
  (void)metrics;
  (void)telemetry;
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

bool SelectionSystem::GetHoverUuidAt(int mouseX, int mouseY, int width,
                                     int height, HoverPickTarget target,
                                     std::string &outUuid,
                                     bool confirmDepth) {
  const auto queryStart = std::chrono::steady_clock::now();
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;

  const bool useIdBuffer = IsIdBufferPickingEnabled(cfg);
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  QueryMetrics metrics;
  std::string bestUuid;

  if (useIdBuffer) {
    std::string pickedUuid;
    const ISelectionContext::PickReadResult pickResult =
        m_controller.ReadPickUuidAtDetailed(mouseX, mouseY, width, height,
                                            hiddenLayers, pickedUuid);
    if (pickResult == ISelectionContext::PickReadResult::Hit &&
        IsUuidValidForTarget(pickedUuid, target, hiddenLayers, m_controller,
                             nullptr)) {
      metrics.usedIdPath = true;
      outUuid = pickedUuid;
      metrics.total = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - queryStart);
      LogQueryMetrics("hover_uuid(id)", metrics, m_queryTelemetry,
                      ++m_queryCounter);
      return true;
    }
    if (pickResult != ISelectionContext::PickReadResult::Unavailable) {
      outUuid.clear();
      return false;
    }
  }

  const auto projectionStart = std::chrono::steady_clock::now();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();
  const bool projectionChanged = !ProjectionMatchesCache(projection, m_queryCache);
  if (projectionChanged) {
    CacheProjectionSnapshot(projection, m_queryCache);
    m_queryCache.hiddenLayers.clear();
  }
  metrics.reprojected = projectionChanged;
  metrics.projection = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - projectionStart);

  Ray mouseRay;
  if (!BuildMouseRay(mouseX, mouseY, height, projection, mouseRay))
    return false;

  if (projectionChanged || hiddenLayers != m_queryCache.hiddenLayers) {
    m_queryCache.hiddenLayers = hiddenLayers;
    m_queryCache.visibleSet = nullptr;
  }

  const auto depthReadStart = std::chrono::steady_clock::now();
  bool hasDepthWorldPoint = false;
  std::array<double, 3> depthWorldPoint{};
  const bool allowDepthConfirmation = ShouldConfirmDepthForQuery(confirmDepth, cfg);
  const bool shouldReadDepth = allowDepthConfirmation &&
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
  if (allowDepthConfirmation) {
    hasDepthWorldPoint = m_queryCache.hasDepthWorldPoint;
    depthWorldPoint = m_queryCache.depthWorldPoint;
  }
  metrics.depthRead = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - depthReadStart);

  if (!m_queryCache.visibleSet) {
    m_queryCache.visibleSet =
        &m_controller.GetVisibleSet(m_queryCache.frustum, m_queryCache.hiddenLayers,
                                    true, 0.0f);
  }
  const ISelectionContext::VisibleSet &visibleSet = *m_queryCache.visibleSet;
  const std::vector<std::string> &candidateUuids =
      VisibleUuidsForTarget(visibleSet, target);

  double bestHitDistance = DBL_MAX;
  bool found = false;
  const auto candidateLoopStart = std::chrono::steady_clock::now();
  for (const auto &uuid : candidateUuids) {
    const ISelectionContext::BoundingBox *bbPtr = nullptr;
    if (!IsUuidValidForTarget(uuid, target, hiddenLayers, m_controller, &bbPtr))
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
      bestHitDistance = candidateScore;
      bestUuid = uuid;
      found = true;
    }
  }
  metrics.candidateLoop = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - candidateLoopStart);
  metrics.usedFallbackPath = true;
  metrics.total = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - queryStart);
  LogQueryMetrics("hover_uuid(fallback)", metrics, m_queryTelemetry,
                  ++m_queryCounter);

  if (found) {
    outUuid = bestUuid;
    return true;
  }
  outUuid.clear();
  return false;
}

bool SelectionSystem::GetFixtureLabelAt(int mouseX, int mouseY, int width,
                                        int height, wxString &outLabel,
                                        wxPoint &outPos,
                                        std::string *outUuid,
                                        bool confirmDepth) {
  const auto queryStart = std::chrono::steady_clock::now();
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;
  const bool useIdBuffer = IsIdBufferPickingEnabled(cfg);
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  QueryMetrics metrics;
  bool found = false;
  std::string bestUuid;

  if (useIdBuffer) {
    std::string pickedUuid;
    const ISelectionContext::PickReadResult pickResult =
        m_controller.ReadPickUuidAtDetailed(mouseX, mouseY, width, height,
                                            hiddenLayers, pickedUuid);
    if (pickResult == ISelectionContext::PickReadResult::Hit) {
      const auto &fixtures = SceneDataManager::Instance().GetFixtures();
      auto fixtureIt = fixtures.find(pickedUuid);
      if (fixtureIt != fixtures.end()) {
        const auto bbIt = m_controller.GetFixtureBoundsMap().find(pickedUuid);
        if (bbIt != m_controller.GetFixtureBoundsMap().end()) {
          const ProjectionSnapshot projection = CaptureProjectionSnapshot();
          if (ProjectBoundingBoxCenter(bbIt->second, projection, height, outPos)) {
            const auto &f = fixtureIt->second;
            bool showName = cfg.GetFloat("label_show_name") != 0.0f;
            bool showId = cfg.GetFloat("label_show_id") != 0.0f;
            bool showDmx = cfg.GetFloat("label_show_dmx") != 0.0f;
            wxString label;
            if (showName)
              label = f.instanceName.empty() ? wxString::FromUTF8(pickedUuid)
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
            outLabel = label;
            bestUuid = pickedUuid;
            if (outUuid)
              *outUuid = pickedUuid;
            found = true;
            metrics.usedIdPath = true;
          }
        }
      }
    }
    if (!found && pickResult != ISelectionContext::PickReadResult::Unavailable) {
      if (outUuid)
        outUuid->clear();
      return false;
    }
    if (found) {
      m_queryTelemetry.totalQueries++;
      m_queryTelemetry.idPathQueries++;
      if (!m_queryTelemetry.hadFixtureResult ||
          m_queryTelemetry.lastFixtureUuid != bestUuid) {
        m_queryTelemetry.repaintEstimates++;
      }
      m_queryTelemetry.hadFixtureResult = true;
      m_queryTelemetry.lastFixtureUuid = bestUuid;
      metrics.total = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - queryStart);
      LogQueryMetrics("fixture", metrics, m_queryTelemetry, ++m_queryCounter);
      return true;
    }
  }

  const auto projectionStart = std::chrono::steady_clock::now();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();
  const bool projectionChanged = !ProjectionMatchesCache(projection, m_queryCache);
  if (projectionChanged) {
    CacheProjectionSnapshot(projection, m_queryCache);
    m_queryCache.hiddenLayers.clear();
  }
  metrics.reprojected = projectionChanged;
  metrics.projection = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - projectionStart);

  Ray mouseRay;
  if (!BuildMouseRay(mouseX, mouseY, height, projection, mouseRay))
    return false;
  if (projectionChanged || hiddenLayers != m_queryCache.hiddenLayers) {
    m_queryCache.hiddenLayers = hiddenLayers;
    m_queryCache.visibleSet = nullptr;
  }
  const auto depthReadStart = std::chrono::steady_clock::now();
  bool hasDepthWorldPoint = false;
  std::array<double, 3> depthWorldPoint{};
  const bool allowDepthConfirmation = ShouldConfirmDepthForQuery(confirmDepth, cfg);
  const bool shouldReadDepth = allowDepthConfirmation &&
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
  if (allowDepthConfirmation) {
    hasDepthWorldPoint = m_queryCache.hasDepthWorldPoint;
    depthWorldPoint = m_queryCache.depthWorldPoint;
  }
  metrics.depthRead = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - depthReadStart);
  bool showName = cfg.GetFloat("label_show_name") != 0.0f;
  bool showId = cfg.GetFloat("label_show_id") != 0.0f;
  bool showDmx = cfg.GetFloat("label_show_dmx") != 0.0f;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();

  double bestHitDistance = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;

  if (!m_queryCache.visibleSet) {
    m_queryCache.visibleSet =
        &m_controller.GetVisibleSet(m_queryCache.frustum, m_queryCache.hiddenLayers,
                                    true, 0.0f);
  }
  const ISelectionContext::VisibleSet &visibleSet = *m_queryCache.visibleSet;

  const auto candidateLoopStart = std::chrono::steady_clock::now();
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

      wxPoint projectedCenter;
      if (!ProjectBoundingBoxCenter(*bbPtr, projection, height, projectedCenter))
        continue;

      bestHitDistance = candidateScore;
      bestPos = projectedCenter;
      bestLabel = label;
      bestUuid = uuid;
      found = true;
    }
  }
  metrics.candidateLoop = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - candidateLoopStart);
  metrics.usedFallbackPath = true;
  metrics.total = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - queryStart);
  m_queryTelemetry.totalQueries++;
  m_queryTelemetry.fallbackQueries++;
  if (projectionChanged)
    m_queryTelemetry.reprojections++;

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
    if (!m_queryTelemetry.hadFixtureResult ||
        m_queryTelemetry.lastFixtureUuid != bestUuid) {
      m_queryTelemetry.repaintEstimates++;
    }
    m_queryTelemetry.hadFixtureResult = true;
    m_queryTelemetry.lastFixtureUuid = bestUuid;
  } else {
    if (m_queryTelemetry.hadFixtureResult)
      m_queryTelemetry.repaintEstimates++;
    m_queryTelemetry.hadFixtureResult = false;
    m_queryTelemetry.lastFixtureUuid.clear();
  }
  LogQueryMetrics("fixture", metrics, m_queryTelemetry, ++m_queryCounter);
  return found;
}

// Finds the best truss label under the mouse cursor.
bool SelectionSystem::GetTrussLabelAt(int mouseX, int mouseY, int width,
                                      int height, wxString &outLabel,
                                      wxPoint &outPos,
                                      std::string *outUuid,
                                      bool confirmDepth) {
  const auto queryStart = std::chrono::steady_clock::now();
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;
  const bool useIdBuffer = IsIdBufferPickingEnabled(cfg);
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  QueryMetrics metrics;
  bool found = false;
  std::string bestUuid;
  if (useIdBuffer) {
    std::string pickedUuid;
    const ISelectionContext::PickReadResult pickResult =
        m_controller.ReadPickUuidAtDetailed(mouseX, mouseY, width, height,
                                            hiddenLayers, pickedUuid);
    if (pickResult == ISelectionContext::PickReadResult::Hit) {
      const auto &trusses = SceneDataManager::Instance().GetTrusses();
      auto trussIt = trusses.find(pickedUuid);
      if (trussIt != trusses.end()) {
        const auto bbIt = m_controller.GetTrussBoundsMap().find(pickedUuid);
        if (bbIt != m_controller.GetTrussBoundsMap().end()) {
          const ProjectionSnapshot projection = CaptureProjectionSnapshot();
          if (ProjectBoundingBoxCenter(bbIt->second, projection, height, outPos)) {
            const auto &t = trussIt->second;
            outLabel = wxString::FromUTF8(t.name);
            bestUuid = pickedUuid;
            if (outUuid)
              *outUuid = pickedUuid;
            found = true;
            metrics.usedIdPath = true;
          }
        }
      }
    }
    if (!found && pickResult != ISelectionContext::PickReadResult::Unavailable) {
      if (outUuid)
        outUuid->clear();
      return false;
    }
    if (found) {
      m_queryTelemetry.totalQueries++;
      m_queryTelemetry.idPathQueries++;
      if (!m_queryTelemetry.hadTrussResult ||
          m_queryTelemetry.lastTrussUuid != bestUuid) {
        m_queryTelemetry.repaintEstimates++;
      }
      m_queryTelemetry.hadTrussResult = true;
      m_queryTelemetry.lastTrussUuid = bestUuid;
      metrics.total = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - queryStart);
      LogQueryMetrics("truss", metrics, m_queryTelemetry, ++m_queryCounter);
      return true;
    }
  }

  const auto projectionStart = std::chrono::steady_clock::now();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();
  const bool projectionChanged = !ProjectionMatchesCache(projection, m_queryCache);
  if (projectionChanged) {
    CacheProjectionSnapshot(projection, m_queryCache);
    m_queryCache.hiddenLayers.clear();
  }
  metrics.reprojected = projectionChanged;
  metrics.projection = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - projectionStart);
  Ray mouseRay;
  if (!BuildMouseRay(mouseX, mouseY, height, projection, mouseRay))
    return false;
  if (projectionChanged || hiddenLayers != m_queryCache.hiddenLayers) {
    m_queryCache.hiddenLayers = hiddenLayers;
    m_queryCache.visibleSet = nullptr;
  }
  const auto depthReadStart = std::chrono::steady_clock::now();
  bool hasDepthWorldPoint = false;
  std::array<double, 3> depthWorldPoint{};
  const bool allowDepthConfirmation = ShouldConfirmDepthForQuery(confirmDepth, cfg);
  const bool shouldReadDepth = allowDepthConfirmation &&
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
  if (allowDepthConfirmation) {
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
  double bestHitDistance = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  const auto candidateLoopStart = std::chrono::steady_clock::now();
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
      bestLabel = t.name.empty() ? wxString::FromUTF8(uuid)
                                 : wxString::FromUTF8(t.name);
      float baseHeight = t.transform.o[2] - t.heightMm * 0.5f;
      const std::string hStr = FormatDistanceLabel(baseHeight, cfg);
      bestLabel += "\nh = " + wxString::FromUTF8(hStr);
      bestUuid = uuid;
      found = true;
    }
  }
  metrics.candidateLoop = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - candidateLoopStart);
  metrics.usedFallbackPath = true;
  metrics.total = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - queryStart);
  m_queryTelemetry.totalQueries++;
  m_queryTelemetry.fallbackQueries++;
  if (projectionChanged)
    m_queryTelemetry.reprojections++;

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
    if (!m_queryTelemetry.hadTrussResult ||
        m_queryTelemetry.lastTrussUuid != bestUuid) {
      m_queryTelemetry.repaintEstimates++;
    }
    m_queryTelemetry.hadTrussResult = true;
    m_queryTelemetry.lastTrussUuid = bestUuid;
  } else {
    if (m_queryTelemetry.hadTrussResult)
      m_queryTelemetry.repaintEstimates++;
    m_queryTelemetry.hadTrussResult = false;
    m_queryTelemetry.lastTrussUuid.clear();
  }
  LogQueryMetrics("truss", metrics, m_queryTelemetry, ++m_queryCounter);
  return found;
}

bool SelectionSystem::GetSceneObjectLabelAt(int mouseX, int mouseY, int width,
                                            int height, wxString &outLabel,
                                            wxPoint &outPos,
                                            std::string *outUuid,
                                            bool confirmDepth) {
  const auto queryStart = std::chrono::steady_clock::now();
  ConfigManager &cfg = ConfigManager::Get();
  if (m_controller.IsCameraMoving() && IsFastInteractionModeEnabled(cfg))
    return false;
  const bool useIdBuffer = IsIdBufferPickingEnabled(cfg);
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  QueryMetrics metrics;
  bool found = false;
  std::string bestUuid;
  if (useIdBuffer) {
    std::string pickedUuid;
    const ISelectionContext::PickReadResult pickResult =
        m_controller.ReadPickUuidAtDetailed(mouseX, mouseY, width, height,
                                            hiddenLayers, pickedUuid);
    if (pickResult == ISelectionContext::PickReadResult::Hit) {
      const auto &sceneObjects = SceneDataManager::Instance().GetSceneObjects();
      auto objectIt = sceneObjects.find(pickedUuid);
      if (objectIt != sceneObjects.end()) {
        const auto bbIt = m_controller.GetObjectBoundsMap().find(pickedUuid);
        if (bbIt != m_controller.GetObjectBoundsMap().end()) {
          const ProjectionSnapshot projection = CaptureProjectionSnapshot();
          if (ProjectBoundingBoxCenter(bbIt->second, projection, height, outPos)) {
            outLabel = wxString::FromUTF8(objectIt->second.name);
            bestUuid = pickedUuid;
            if (outUuid)
              *outUuid = pickedUuid;
            found = true;
            metrics.usedIdPath = true;
          }
        }
      }
    }
    if (!found && pickResult != ISelectionContext::PickReadResult::Unavailable) {
      if (outUuid)
        outUuid->clear();
      return false;
    }
    if (found) {
      m_queryTelemetry.totalQueries++;
      m_queryTelemetry.idPathQueries++;
      if (!m_queryTelemetry.hadObjectResult ||
          m_queryTelemetry.lastObjectUuid != bestUuid) {
        m_queryTelemetry.repaintEstimates++;
      }
      m_queryTelemetry.hadObjectResult = true;
      m_queryTelemetry.lastObjectUuid = bestUuid;
      metrics.total = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - queryStart);
      LogQueryMetrics("object", metrics, m_queryTelemetry, ++m_queryCounter);
      return true;
    }
  }

  const auto projectionStart = std::chrono::steady_clock::now();
  const ProjectionSnapshot projection = CaptureProjectionSnapshot();
  const bool projectionChanged = !ProjectionMatchesCache(projection, m_queryCache);
  if (projectionChanged) {
    CacheProjectionSnapshot(projection, m_queryCache);
    m_queryCache.hiddenLayers.clear();
  }
  metrics.reprojected = projectionChanged;
  metrics.projection = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - projectionStart);
  Ray mouseRay;
  if (!BuildMouseRay(mouseX, mouseY, height, projection, mouseRay))
    return false;
  if (projectionChanged || hiddenLayers != m_queryCache.hiddenLayers) {
    m_queryCache.hiddenLayers = hiddenLayers;
    m_queryCache.visibleSet = nullptr;
  }
  const auto depthReadStart = std::chrono::steady_clock::now();
  bool hasDepthWorldPoint = false;
  std::array<double, 3> depthWorldPoint{};
  const bool allowDepthConfirmation = ShouldConfirmDepthForQuery(confirmDepth, cfg);
  const bool shouldReadDepth = allowDepthConfirmation &&
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
  if (allowDepthConfirmation) {
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
  double bestHitDistance = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  const auto candidateLoopStart = std::chrono::steady_clock::now();
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
      bestLabel = o.name.empty() ? wxString::FromUTF8(uuid)
                                 : wxString::FromUTF8(o.name);
      bestUuid = uuid;
      found = true;
    }
  }
  metrics.candidateLoop = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - candidateLoopStart);
  metrics.usedFallbackPath = true;
  metrics.total = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - queryStart);
  m_queryTelemetry.totalQueries++;
  m_queryTelemetry.fallbackQueries++;
  if (projectionChanged)
    m_queryTelemetry.reprojections++;

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
    if (!m_queryTelemetry.hadObjectResult ||
        m_queryTelemetry.lastObjectUuid != bestUuid) {
      m_queryTelemetry.repaintEstimates++;
    }
    m_queryTelemetry.hadObjectResult = true;
    m_queryTelemetry.lastObjectUuid = bestUuid;
  } else {
    if (m_queryTelemetry.hadObjectResult)
      m_queryTelemetry.repaintEstimates++;
    m_queryTelemetry.hadObjectResult = false;
    m_queryTelemetry.lastObjectUuid.clear();
  }
  LogQueryMetrics("object", metrics, m_queryTelemetry, ++m_queryCounter);
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
