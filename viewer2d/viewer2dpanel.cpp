/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
/*
 * File: viewer2dpanel.cpp
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Implementation of a top-down OpenGL viewer sharing 3D models.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#include "gl_context_utils.h"
// macOS uses the OpenGL framework headers; guard includes for cross-platform
// builds.
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "../gui/mainwindow/ids/tools_ids.h"
#include "../gui/selection_origin_token.h"
#include "../viewer_common/gl_canvas_config.h"
#include "../viewer_common/gl_framebuffer_capture_target.h"
#include "../viewer_common/measure_overlay_style.h"
#include "../viewport_interaction_scope.h"
#include "canvas2d.h"
#include "configmanager.h"
#include "continuous_placement_scene.h"
#include "diagnostics/DiagnosticReport.h"
#include "editable_focus_utils.h"
#include "fixturepatchdialog.h"
#include "fixturetablepanel.h"
#include "gl_state_guard.h"
#include "hoisttablepanel.h"
#include "logger.h"
#include "mainwindow.h"
#include "positionvalueupdate.h"
#include "scene_grouping.h"
#include "scene_object_primitive_editing.h"
#include "sceneobjecttablepanel.h"
#include "selection_movement_settings.h"
#include "trusstablepanel.h"
#include "ui_render_size.h"
#include "units/units.h"
#include "viewer2d_ruler_overlay.h"
#include "viewer2d_support_selection.h"
#include "viewer2dpanel.h"
#include "viewer2dpanel_helpers.h"
#include "viewer2drenderpanel.h"
#include "viewer2dviewfit.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <utility>
#include <vector>
#include <wx/app.h>
#include <wx/debug.h>
#include <wx/log.h>
#include <wx/utils.h>

// Pixels per meter at default zoom level.
static constexpr float PIXELS_PER_METER = 25.0f;

// Reports whether viewport interactions should ignore the active table scope.
static bool IsCrossTableViewportActionsEnabled() {
  return ConfigManager::Get().GetValue(
             viewport_interaction_scope::kCrossTableActionsConfigKey) == "1";
}

namespace {
constexpr size_t kMaxCapturePixels = 8192u * 8192u;
constexpr size_t kMaxCaptureBytes = 64u * 1024u * 1024u;

class ScopedReadBufferPackAlignmentState {
public:
  // Captures read-buffer and pack-alignment state before a pixel read.
  ScopedReadBufferPackAlignmentState() {
    glGetIntegerv(GL_READ_BUFFER, &readBuffer_);
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment_);
  }

  ScopedReadBufferPackAlignmentState(
      const ScopedReadBufferPackAlignmentState &) = delete;
  ScopedReadBufferPackAlignmentState &
  operator=(const ScopedReadBufferPackAlignmentState &) = delete;

  // Restores read-buffer and pack-alignment state after a pixel read.
  ~ScopedReadBufferPackAlignmentState() {
    glReadBuffer(static_cast<GLenum>(readBuffer_));
    glPixelStorei(GL_PACK_ALIGNMENT, packAlignment_);
  }

private:
  GLint readBuffer_ = GL_BACK;
  GLint packAlignment_ = 4;
};

wxRect BuildSelectionRectDirtyRegion(const wxPoint &a, const wxPoint &b) {
  const int x = std::min(a.x, b.x);
  const int y = std::min(a.y, b.y);
  const int w = std::max(1, std::abs(a.x - b.x) + 1);
  const int h = std::max(1, std::abs(a.y - b.y) + 1);
  wxRect rect(x, y, w, h);
  rect.Inflate(2);
  return rect;
}

wxRect BuildPointDirtyRegion(const wxPoint &point, int radius) {
  const int size = std::max(1, radius * 2);
  return wxRect(point.x - radius, point.y - radius, size, size);
}

// Normalizes label rotation so text stays parallel to the line without
// appearing upside down.
float NormalizeMeasureLabelAngleDegrees(float angleDegrees) {
  while (angleDegrees > 90.0f)
    angleDegrees -= 180.0f;
  while (angleDegrees < -90.0f)
    angleDegrees += 180.0f;
  return angleDegrees;
}

// Resolves the center world position for any selectable scene element UUID.
std::optional<std::array<float, 3>>
ResolveSceneElementCenterByUuid(const ConfigManager &cfg,
                                const std::string &uuid) {
  const auto toMeters = [](const std::array<float, 3> &pointMm) {
    return std::array<float, 3>{pointMm[0] / 1000.0f, pointMm[1] / 1000.0f,
                                pointMm[2] / 1000.0f};
  };
  const auto &scene = cfg.GetScene();
  if (const auto fit = scene.fixtures.find(uuid); fit != scene.fixtures.end())
    return toMeters(fit->second.GetPosition());
  if (const auto tit = scene.trusses.find(uuid); tit != scene.trusses.end())
    return toMeters(tit->second.transform.o);
  if (const auto sit = scene.supports.find(uuid); sit != scene.supports.end())
    return toMeters(sit->second.transform.o);
  if (const auto oit = scene.sceneObjects.find(uuid);
      oit != scene.sceneObjects.end())
    return toMeters(oit->second.transform.o);
  return std::nullopt;
}

// Finds cached world bounds for any selectable scene element UUID.
std::optional<ISelectionContext::BoundingBox>
ResolveSceneElementBoundsByUuid(const ISelectionContext &selectionContext,
                                const ConfigManager &cfg,
    const std::string &uuid) {
  if (const auto *bounds = selectionContext.FindFixtureBounds(uuid))
    return *bounds;
  if (const auto *bounds = selectionContext.FindTrussBounds(uuid))
    return *bounds;
  if (const auto *bounds = selectionContext.FindObjectBounds(uuid))
    return *bounds;
  if (const auto center = ResolveSceneElementCenterByUuid(cfg, uuid))
    return ISelectionContext::BoundingBox{*center, *center};
  return std::nullopt;
}

// Computes nearest points between two bounds in the active 2D projection plane.
std::pair<std::array<float, 3>, std::array<float, 3>>
ComputeNearestProjectedBoundsPoints(const ISelectionContext::BoundingBox &a,
                                    const ISelectionContext::BoundingBox &b,
                                    Viewer2DView view) {
  std::array<float, 3> start{0.5f * (a.min[0] + a.max[0]),
                             0.5f * (a.min[1] + a.max[1]),
                             0.5f * (a.min[2] + a.max[2])};
  std::array<float, 3> end{0.5f * (b.min[0] + b.max[0]),
                           0.5f * (b.min[1] + b.max[1]),
                           0.5f * (b.min[2] + b.max[2])};
  const auto assignAxis = [&](int axis) {
    if (a.max[axis] < b.min[axis]) {
      start[axis] = a.max[axis];
      end[axis] = b.min[axis];
    } else if (b.max[axis] < a.min[axis]) {
      start[axis] = a.min[axis];
      end[axis] = b.max[axis];
    } else {
      const float overlap = std::max(a.min[axis], b.min[axis]);
      start[axis] = overlap;
      end[axis] = overlap;
    }
  };
  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    assignAxis(0);
    assignAxis(1);
    break;
  case Viewer2DView::Front:
    assignAxis(0);
    assignAxis(2);
    break;
  case Viewer2DView::Side:
    assignAxis(1);
    assignAxis(2);
    break;
  }
  return {start, end};
}

// Draws a 2D triangular arrowhead in screen-space overlay coordinates.
void DrawSelectionDragArrowhead2D(float baseX, float baseY, float dirX,
                                  float dirY, float length, float radius) {
  const float tipX = baseX + dirX * length;
  const float tipY = baseY + dirY * length;
  const float sideX = -dirY;
  const float sideY = dirX;

  glBegin(GL_TRIANGLES);
  glVertex2f(tipX, tipY);
  glVertex2f(baseX + sideX * radius, baseY + sideY * radius);
  glVertex2f(baseX - sideX * radius, baseY - sideY * radius);
  glEnd();
}

// Draws a CAD-like temporary dimension overlay with extension lines, arrows,
// and label.
void DrawMeasureOverlay(
    Viewer3DController &controller,
                        const Viewer2DMeasureToolState &measureState,
    const std::array<float, 3> &targetWorld, Viewer2DView view, int width,
    int height, float zoom, float offsetX, float offsetY,
    Units::DistanceUnitSystem distanceUnitSystem, bool darkMode,
                        const std::optional<std::array<float, 2>> &targetScreenOverride) {
  const auto startPx =
      Viewer2DMeasureWorldToScreen(measureState.anchorMeasureWorld, view, width,
                                                    height, zoom, offsetX, offsetY);
  const auto endPx =
      targetScreenOverride.has_value()
                         ? targetScreenOverride
          : Viewer2DMeasureWorldToScreen(targetWorld, view, width, height, zoom,
                                         offsetX, offsetY);
  if (!startPx || !endPx)
    return;

  // Computes the displayed distance using mouse screen delta during live
  // preview for axis-safe tracking.
  float distanceMeters = 0.0f;
  if (targetScreenOverride.has_value()) {
    const float dxPixels = (*endPx)[0] - (*startPx)[0];
    const float dyPixels = (*endPx)[1] - (*startPx)[1];
    const float pixelsPerMeter = PIXELS_PER_METER * zoom;
    if (pixelsPerMeter > 0.0f)
      distanceMeters =
          std::sqrt(dxPixels * dxPixels + dyPixels * dyPixels) / pixelsPerMeter;
  } else {
    // Compute distance in the active 2D projection plane instead of full 3D
    // space.
    float du = 0.0f;
    float dv = 0.0f;
    switch (view) {
    case Viewer2DView::Top:
    case Viewer2DView::Bottom:
      du = targetWorld[0] - measureState.anchorMeasureWorld[0];
      dv = targetWorld[1] - measureState.anchorMeasureWorld[1];
      break;
    case Viewer2DView::Front:
      du = targetWorld[0] - measureState.anchorMeasureWorld[0];
      dv = targetWorld[2] - measureState.anchorMeasureWorld[2];
      break;
    case Viewer2DView::Side:
      du = targetWorld[1] - measureState.anchorMeasureWorld[1];
      dv = targetWorld[2] - measureState.anchorMeasureWorld[2];
      break;
    }
    distanceMeters = std::sqrt(du * du + dv * dv);
  }
  const std::string text =
      Units::FormatDistanceFromMillimeters(
      static_cast<double>(distanceMeters) * 1000.0, distanceUnitSystem,
      Units::ValueFormatContext::Label) +
                           " " + Units::DistanceUnitSuffix(distanceUnitSystem);

  const float x0 = (*startPx)[0];
  const float y0 = static_cast<float>(height) - (*startPx)[1];
  const float x1 = (*endPx)[0];
  const float y1 = static_cast<float>(height) - (*endPx)[1];
  float vx = x1 - x0;
  float vy = y1 - y0;
  const float len = std::sqrt(vx * vx + vy * vy);
  if (len < 1.0f)
    return;
  vx /= len;
  vy /= len;
  const float nx = -vy;
  const float ny = vx;
  const float offset = 14.0f;
  const float tx0 = x0 + nx * offset;
  const float ty0 = y0 + ny * offset;
  const float tx1 = x1 + nx * offset;
  const float ty1 = y1 + ny * offset;
  const float labelX = ((tx0 + tx1) * 0.5f);
  const float labelY =
      static_cast<float>(height) - ((ty0 + ty1) * 0.5f) - 10.0f;
  const float labelAngleDegrees = NormalizeMeasureLabelAngleDegrees(
      -std::atan2(ty1 - ty0, tx1 - tx0) * (180.0f / 3.14159265358979323846f));
  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height),
          -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  viewer_common::DrawMeasureOverlayStyle(x0, y0, x1, y1, darkMode);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_DEPTH_TEST);

  std::vector<OverlayTextLabel> labels{{labelX, labelY, text, true, true,
                                        3.0f * zoom, true, 0.95f, 0.1f, 0.1f,
       labelAngleDegrees}};
  controller.DrawOverlayTextLabels(labels, darkMode);
}

void ValidateGlStateAfterRender(const char *stage, int expectedWidth,
                                int expectedHeight) {
  GLint framebuffer = 0;
  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  glGetIntegerv(GL_VIEWPORT, viewport);
  const bool validFramebuffer = framebuffer == 0;
  const bool validViewport = viewport[0] == 0 && viewport[1] == 0 &&
                             viewport[2] == expectedWidth &&
      viewport[3] == expectedHeight;
  if (!validFramebuffer || !validViewport) {
    wxLogTrace("viewer2d_gl_state",
               "%s left unexpected GL state (fbo=%d viewport=%d,%d,%d,%d "
               "expected=0,0,%d,%d).",
               stage, framebuffer, viewport[0], viewport[1], viewport[2],
               viewport[3], expectedWidth, expectedHeight);
  }
  wxASSERT_MSG(validFramebuffer,
               "Unexpected non-default framebuffer after 2D render.");
  wxASSERT_MSG(validViewport, "Unexpected viewport after 2D render.");
}

bool TryAllocateCaptureBuffer(std::vector<unsigned char> &pixels, int width,
                              int height) {
  if (width <= 0 || height <= 0)
    return false;
  const size_t totalPixels =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  const size_t totalBytes = totalPixels * 4;
  if (totalPixels > kMaxCapturePixels || totalBytes > kMaxCaptureBytes) {
    Logger::Instance().Log("Viewer2DPanel: capture buffer too large (" +
                           std::to_string(width) + "x" +
                           std::to_string(height) + ").");
    return false;
  }
  try {
    pixels.assign(totalBytes, 0);
  } catch (const std::bad_alloc &) {
    Logger::Instance().Log("Viewer2DPanel: capture buffer allocation failed.");
    return false;
  }
  return true;
}

CanvasStroke MakeGridStroke(float r, float g, float b) {
  CanvasStroke stroke;
  stroke.color = {r, g, b, 1.0f};
  stroke.width = 1.0f;
  return stroke;
}

// Converts a logical canvas point to framebuffer coordinates using content
// scale.
wxPoint ToFramebufferPoint(wxWindow *window, const wxPoint &logicalPoint) {
  if (window == nullptr)
    return logicalPoint;
  const double contentScale =
      static_cast<double>(window->GetContentScaleFactor());
  if (!std::isfinite(contentScale) || contentScale <= 0.0)
    return logicalPoint;
  return wxPoint(static_cast<int>(std::lround(
                     static_cast<double>(logicalPoint.x) * contentScale)),
                 static_cast<int>(std::lround(
                     static_cast<double>(logicalPoint.y) * contentScale)));
}

// Converts framebuffer-space mouse coordinates back to logical window
// coordinates.
wxPoint
ToLogicalPointFromFramebuffer(wxWindow *window,
                                      const std::array<float, 2> &framebufferPoint) {
  if (window == nullptr)
    return wxPoint(static_cast<int>(std::lround(framebufferPoint[0])),
                   static_cast<int>(std::lround(framebufferPoint[1])));
  const double contentScale =
      static_cast<double>(window->GetContentScaleFactor());
  if (!std::isfinite(contentScale) || contentScale <= 0.0) {
    return wxPoint(static_cast<int>(std::lround(framebufferPoint[0])),
                   static_cast<int>(std::lround(framebufferPoint[1])));
  }
  return wxPoint(
      static_cast<int>(std::lround(framebufferPoint[0] / contentScale)),
                 static_cast<int>(std::lround(framebufferPoint[1] / contentScale)));
}

// Emit grid primitives to a canvas so the command buffer records the same
// visual information shown by the OpenGL renderer. Coordinates are expressed in
// the 2D world space after the camera orientation has been applied.
void EmitGrid(ICanvas2D &canvas, int style, Viewer2DView view, float r, float g,
              float b) {
  const float size = 20.0f;
  const float step = 1.0f;
  auto stroke = MakeGridStroke(r, g, b);

  if (style == 0) {
    for (float i = -size; i <= size; i += step) {
      switch (view) {
      case Viewer2DView::Top:
      case Viewer2DView::Bottom:
        canvas.DrawLine(i, -size, i, size, stroke);
        canvas.DrawLine(-size, i, size, i, stroke);
        break;
      case Viewer2DView::Front:
        canvas.DrawLine(i, -size, i, size, stroke);
        canvas.DrawLine(-size, i, size, i, stroke);
        break;
      case Viewer2DView::Side:
        canvas.DrawLine(i, -size, i, size, stroke);
        canvas.DrawLine(-size, i, size, i, stroke);
        break;
      }
    }
  } else if (style == 1) {
    for (float x = -size; x <= size; x += step) {
      for (float y = -size; y <= size; y += step) {
        std::vector<float> pt = {x, y};
        canvas.DrawCircle(pt[0], pt[1], 0.05f, stroke, nullptr);
      }
    }
  } else {
    float half = step * 0.1f;
    for (float x = -size; x <= size; x += step) {
      for (float y = -size; y <= size; y += step) {
        canvas.DrawLine(x - half, y, x + half, y, stroke);
        canvas.DrawLine(x, y - half, x, y + half, stroke);
      }
    }
  }
}

// Builds fixture UUIDs filtered by type name for selection workflows.
std::vector<std::string>
BuildFixtureSelectionByType(const MvrScene &scene,
                            const std::string &typeName) {
  std::vector<std::string> uuids;
  uuids.reserve(scene.fixtures.size());
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (typeName.empty() || fixture.typeName == typeName)
      uuids.push_back(uuid);
  }
  return uuids;
}

// Builds fixture UUIDs filtered by position-name mapping criteria.
std::vector<std::string>
BuildFixtureSelectionByPosition(const MvrScene &scene,
                                const std::string &positionName,
    bool selectNoPosition) {
  std::vector<std::string> uuids;
  uuids.reserve(scene.fixtures.size());
  for (const auto &[uuid, fixture] : scene.fixtures) {
    const bool hasPosition = !fixture.positionName.empty();
    if (selectNoPosition) {
      if (!hasPosition)
        uuids.push_back(uuid);
      continue;
    }
    if (positionName.empty() || fixture.positionName == positionName)
      uuids.push_back(uuid);
  }
  return uuids;
}

void ApplyFixtureSelectionToUi(const std::vector<std::string> &selection,
                               Viewer3DController &controller) {
  ConfigManager &cfg = ConfigManager::Get();
  if (selection != cfg.GetSelectedFixtures()) {
    cfg.PushUndoState("fixture selection");
    cfg.SetSelectedFixtures(selection);
    if (Viewer2DRenderPanel::Instance())
      Viewer2DRenderPanel::Instance()->RefreshLabelControlsFromSelection();
  }
  controller.SetSelectedUuids(selection);
  selection::ScopedOrigin selectionOrigin(selection::Origin::Viewer2D);
  if (FixtureTablePanel::Instance()) {
    if (selection.empty())
      FixtureTablePanel::Instance()->ClearSelection();
    else
      FixtureTablePanel::Instance()->SelectByUuid(selection, false);
  }
}

// Returns a canonical resource key used to group trusses by type or source.
std::string BuildTrussModelSelectionKey(const Truss &truss) {
  auto normalize = [](std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
  };

  if (!truss.perastageTypeKey.empty())
    return "perastage-type:" + normalize(truss.perastageTypeKey);
  if (!truss.gdtfSpec.empty())
    return "gdtf:" + normalize(truss.gdtfSpec);
  if (!truss.modelFile.empty())
    return "model-file:" + normalize(truss.modelFile);
  if (!truss.symbolFile.empty())
    return "symbol-file:" + normalize(truss.symbolFile);
  if (!truss.model.empty())
    return "display-model:" + normalize(truss.model);
  return {};
}

// Builds truss UUIDs filtered by model-or-source-file key for selection
// workflows.
std::vector<std::string>
BuildTrussSelectionByModelKey(const MvrScene &scene,
                              const std::string &modelKey) {
  std::vector<std::string> uuids;
  uuids.reserve(scene.trusses.size());
  for (const auto &[uuid, truss] : scene.trusses) {
    if (modelKey.empty() || BuildTrussModelSelectionKey(truss) == modelKey)
      uuids.push_back(uuid);
  }
  return uuids;
}

// Builds truss UUIDs filtered by position-name mapping criteria.
std::vector<std::string>
BuildTrussSelectionByPosition(const MvrScene &scene,
                              const std::string &positionName,
    bool selectNoPosition) {
  std::vector<std::string> uuids;
  uuids.reserve(scene.trusses.size());
  for (const auto &[uuid, truss] : scene.trusses) {
    const bool hasPosition = !truss.positionName.empty();
    if (selectNoPosition) {
      if (!hasPosition)
        uuids.push_back(uuid);
      continue;
    }
    if (positionName.empty() || truss.positionName == positionName)
      uuids.push_back(uuid);
  }
  return uuids;
}

// Applies a truss selection to scene state, controller highlights, and table
// UI.
void ApplyTrussSelectionToUi(const std::vector<std::string> &selection,
                             Viewer3DController &controller) {
  ConfigManager &cfg = ConfigManager::Get();
  if (selection != cfg.GetSelectedTrusses()) {
    cfg.PushUndoState("truss selection");
    cfg.SetSelectedTrusses(selection);
  }
  controller.SetSelectedUuids(selection);
  selection::ScopedOrigin selectionOrigin(selection::Origin::Viewer2D);
  if (TrussTablePanel::Instance()) {
    if (selection.empty())
      TrussTablePanel::Instance()->ClearSelection();
    else
      TrussTablePanel::Instance()->SelectByUuid(selection, false);
  }
}

// Appends missing UUIDs from the added selection while preserving existing
// order.
std::vector<std::string>
MergeSelectionAdditive(std::vector<std::string> selection,
                       const std::vector<std::string> &added) {
  for (const std::string &uuid : added) {
    if (std::find(selection.begin(), selection.end(), uuid) == selection.end())
      selection.push_back(uuid);
  }
  return selection;
}

// Combines all directly selected entity UUID lists into a stable unique
// sequence.
std::vector<std::string> BuildDirectSelection(const ConfigManager &cfg) {
  std::vector<std::string> selection;
  selection = MergeSelectionAdditive(selection, cfg.GetSelectedFixtures());
  selection = MergeSelectionAdditive(selection, cfg.GetSelectedTrusses());
  selection = MergeSelectionAdditive(selection, cfg.GetSelectedSupports());
  return MergeSelectionAdditive(selection, cfg.GetSelectedSceneObjects());
}

// Combines all selected entity UUID lists into a stable unique sequence.
std::vector<std::string> BuildCombinedSelection(const ConfigManager &cfg) {
  const scene_grouping::ObjectSelection selection{
      .fixtures = cfg.GetSelectedFixtures(),
      .trusses = cfg.GetSelectedTrusses(),
      .supports = cfg.GetSelectedSupports(),
      .sceneObjects = cfg.GetSelectedSceneObjects()};
  return scene_grouping::ExpandSelectionForGroupHighlights(
      cfg.GetScene(), selection,
      selection_movement_settings::LoadInteractiveTransformPolicy(cfg));
}

// Builds the viewer highlight selection while preserving other table selections
// during additive edits.
std::vector<std::string>
BuildViewerSelectionForTableSelection(const ConfigManager &cfg,
                                      const std::vector<std::string> &selection,
    bool additive) {
  if (additive)
    return BuildCombinedSelection(cfg);
  return selection;
}

} // namespace

namespace {
Viewer2DPanel *g_instance = nullptr;
constexpr int kInteractionPauseTimerId = wxID_HIGHEST + 220;
constexpr int kHoverHitTestTimerId = wxID_HIGHEST + 221;
} // namespace

wxBEGIN_EVENT_TABLE(Viewer2DPanel, wxGLCanvas) EVT_PAINT(Viewer2DPanel::OnPaint)
    EVT_LEFT_DOWN(Viewer2DPanel::OnMouseDown) EVT_LEFT_UP(
        Viewer2DPanel::OnMouseUp) EVT_MOTION(Viewer2DPanel::OnMouseMove)
        EVT_LEFT_DCLICK(Viewer2DPanel::OnMouseDClick) EVT_MOUSEWHEEL(
            Viewer2DPanel::OnMouseWheel) EVT_RIGHT_UP(Viewer2DPanel::OnRightUp)
            EVT_KEY_DOWN(Viewer2DPanel::OnKeyDown)
                EVT_ENTER_WINDOW(Viewer2DPanel::OnMouseEnter)
                    EVT_LEAVE_WINDOW(Viewer2DPanel::OnMouseLeave)
                        EVT_MOUSE_CAPTURE_LOST(Viewer2DPanel::OnCaptureLost)
                            EVT_TIMER(kInteractionPauseTimerId,
                                      Viewer2DPanel::OnInteractionPauseTimer)
                            EVT_TIMER(kHoverHitTestTimerId,
                                      Viewer2DPanel::OnHoverHitTestTimer)
                                    EVT_SIZE(Viewer2DPanel::OnResize)
                                        wxEND_EVENT_TABLE()

                                            Viewer2DPanel::Viewer2DPanel(
                                                wxWindow *parent,
                                                bool allowOffscreenRender,
                                                bool persistViewState,
                                                bool enableSelection)
    : wxGLCanvas(parent, wxID_ANY, gl_lifecycle::GetStandardCanvasAttributes(),
                 wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
      m_allowOffscreenRender(allowOffscreenRender),
      m_interactionResumeTimer(this, kInteractionPauseTimerId),
      m_hoverHitTestTimer(this, kHoverHitTestTimerId),
      m_persistViewState(persistViewState), m_enableSelection(enableSelection) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  m_controller.SetSelectionOutlineEnabled(m_enableSelection);
  m_magnetEnabled = ConfigManager::Get().GetValue(
                        magnet_snap::kMagnetEnabledConfigKey) == "1";
  m_leftDragSelectionMovementEnabled =
      selection_movement_settings::IsLeftDragSelectionMovementEnabled(
          ConfigManager::Get());
  m_axisConstrainedMovementEnabled =
      selection_movement_settings::IsAxisConstrainedMovementEnabled(
          ConfigManager::Get());
  m_glContext = new wxGLContext(this);
  if (m_enableSelection) {
    StartDragTableUpdateWorker();
  }
}

// Releases timers, interaction state, and GL resources owned by the 2D panel.
Viewer2DPanel::~Viewer2DPanel() {
  if (m_glContext && IsShownOnScreen() &&
      gl_lifecycle::TrySetCurrent(*this, m_glContext, "Viewer2DPanel",
                                  "destructor cleanup")) {
    ReleaseCaptureFramebufferTarget();
  }
  if (HasCapture())
    ReleaseMouse();
  if (g_instance == this)
    g_instance = nullptr;
  m_dragMode = DragMode::None;
  m_dragAxis = DragAxis::None;
  m_dragTarget = DragTarget::None;
  m_dragSelectionUuids.clear();
  m_dragSelectionMoved = false;
  m_pendingMagnetSnap.reset();
  m_rectSelecting = false;
  m_interactionResumeTimer.Stop();
  m_hoverHitTestTimer.Stop();
  StopDragTableUpdateWorker();
  delete m_glContext;
}

Viewer2DPanel *Viewer2DPanel::Instance() { return g_instance; }

void Viewer2DPanel::SetInstance(Viewer2DPanel *panel) { g_instance = panel; }

void Viewer2DPanel::RequestRepaint() {
  if (m_fullRepaintQueued)
    return;
  m_repaintQueued = true;
  m_fullRepaintQueued = true;
  TrackRefreshTelemetry();
  Refresh(false);
}

void Viewer2DPanel::RequestRepaint(const wxRect &dirtyRect) {
  if (!dirtyRect.IsEmpty() && !m_fullRepaintQueued && !m_repaintQueued) {
    m_repaintQueued = true;
    TrackRefreshTelemetry();
    RefreshRect(dirtyRect, false);
    return;
  }
  RequestRepaint();
}

void Viewer2DPanel::ResetRepaintCoalescing() {
  m_repaintQueued = false;
  m_fullRepaintQueued = false;
}

void Viewer2DPanel::TrackRefreshTelemetry() {
#ifndef NDEBUG
  const auto now = std::chrono::steady_clock::now();
  if (m_refreshTelemetryWindowStart.time_since_epoch().count() == 0)
    m_refreshTelemetryWindowStart = now;
  ++m_refreshesInCurrentWindow;
  const auto elapsed = now - m_refreshTelemetryWindowStart;
  if (elapsed >= std::chrono::seconds(1)) {
    wxLogDebug("Viewer2DPanel refreshes/s: %d", m_refreshesInCurrentWindow);
    m_refreshTelemetryWindowStart = now;
    m_refreshesInCurrentWindow = 0;
  }
#endif
}

// Updates cached 2D scene data, selection state, and repaint scheduling.
void Viewer2DPanel::UpdateScene(bool reload) {
  if (reload && m_enableSelection && ShouldPauseHeavyTasks())
    return;

  m_logFirstPickAfterSceneUpdate = true;
  m_lastUpdateSceneReloadRequested = reload;
  if (reload)
    m_controller.Update();
  if (m_enableSelection) {
    ConfigManager &cfg = ConfigManager::Get();
    m_controller.SetSelectedUuids(BuildCombinedSelection(cfg),
                                  BuildDirectSelection(cfg));
  }
  InvalidatePickCache();
  RequestRepaint();
}

void Viewer2DPanel::SetRenderMode(Viewer2DRenderMode mode) {
  m_renderMode = mode;
  RequestRepaint();
}

void Viewer2DPanel::SetView(Viewer2DView view) {
  m_view = view;
  m_viewMotionSinceLastHoverHitTest = true;
  InvalidatePickCache();
  RequestRepaint();
}

void Viewer2DPanel::SetSelectedUuids(
    const std::vector<std::string> &selection) {
  if (!m_enableSelection)
    return;
  const scene_grouping::ObjectSelection typedSelection{.fixtures = selection,
                                                       .trusses = selection,
                                                       .supports = selection,
                                                       .sceneObjects =
                                                           selection};
  const std::vector<std::string> expandedSelection =
      scene_grouping::ExpandSelectionForGroupHighlights(
          ConfigManager::Get().GetScene(), typedSelection,
          selection_movement_settings::LoadInteractiveTransformPolicy(
              ConfigManager::Get()));
  if (expandedSelection == m_lastAppliedSelectionUuids &&
      selection == m_lastAppliedPrimarySelectionUuids)
    return;
  m_lastAppliedSelectionUuids = expandedSelection;
  m_lastAppliedPrimarySelectionUuids = selection;
  m_controller.SetSelectedUuids(expandedSelection, selection);
  RequestRepaint();
}

void Viewer2DPanel::SetLayerColor(const std::string &layer,
                                  const std::string &hex) {
  // Forward the updated color to the shared controller so the 2D view
  // reflects the user's choice immediately.
  m_controller.SetLayerColor(layer, hex);
  InvalidatePickCache();
}

void Viewer2DPanel::LoadViewFromConfig() {
  ConfigManager &cfg = ConfigManager::Get();
  m_offsetX = cfg.GetFloat("view2d_offset_x");
  m_offsetY = cfg.GetFloat("view2d_offset_y");
  m_zoom = cfg.GetFloat("view2d_zoom");
  m_renderMode = static_cast<Viewer2DRenderMode>(
      static_cast<int>(cfg.GetFloat("view2d_render_mode")));
  m_view =
      static_cast<Viewer2DView>(static_cast<int>(cfg.GetFloat("view2d_view")));
}

void Viewer2DPanel::SaveViewToConfig() const {
  if (!m_persistViewState)
    return;
  ConfigManager &cfg = ConfigManager::Get();
  cfg.SetFloat("view2d_offset_x", m_offsetX);
  cfg.SetFloat("view2d_offset_y", m_offsetY);
  cfg.SetFloat("view2d_zoom", m_zoom);
  cfg.SetFloat("view2d_render_mode", static_cast<float>(m_renderMode));
  cfg.SetFloat("view2d_view", static_cast<float>(m_view));
}

void Viewer2DPanel::ApplyViewState(float offsetX, float offsetY, float zoom,
                                   Viewer2DView view,
                                   Viewer2DRenderMode renderMode) {
  m_offsetX = offsetX;
  m_offsetY = offsetY;
  m_zoom = zoom;
  m_view = view;
  m_renderMode = renderMode;
  InvalidatePickCache();
}

bool Viewer2DPanel::FitViewToScene() {
  int width = 0;
  int height = 0;
  GetClientSize(&width, &height);
  viewer2d::ViewFitResult fit;
  if (!viewer2d::ComputeViewFit(m_controller, m_view, width, height, fit))
    return false;

  m_offsetX = fit.offsetXPixels;
  m_offsetY = fit.offsetYPixels;
  m_zoom = fit.zoom;
  if (m_zoom < 0.1f)
    m_zoom = 0.1f;
  if (m_persistViewState)
    SaveViewToConfig();
  InvalidatePickCache();
  RequestRepaint();
  return true;
}

void Viewer2DPanel::RequestFrameCapture() { m_captureNextFrame = true; }

void Viewer2DPanel::CaptureFrameAsync(
    std::function<void(CommandBuffer, Viewer2DViewState)> callback,
    bool useSimplifiedFootprints, bool includeGridInCapture) {
  m_captureCallback = std::move(callback);
  m_useSimplifiedFootprints = useSimplifiedFootprints;
  if (!m_useSimplifiedFootprints &&
      (m_view == Viewer2DView::Front || m_view == Viewer2DView::Side)) {
    // Front/side printouts should still use cached fixture symbols so repeated
    // fixtures are emitted as reusable PDF symbols like in top/bottom views.
    m_useSimplifiedFootprints = true;
  }
  m_captureIncludeGrid = includeGridInCapture;
  RequestFrameCapture();
  RequestRepaint();
}

void Viewer2DPanel::CaptureFrameNow(
    std::function<void(CommandBuffer, Viewer2DViewState)> callback,
    bool useSimplifiedFootprints, bool includeGridInCapture) {
  CaptureFrameAsync(std::move(callback), useSimplifiedFootprints,
                    includeGridInCapture);
  if (m_allowOffscreenRender) {
    m_forceOffscreenRender = true;
    InitGL();
    Render();
    m_forceOffscreenRender = false;
    return;
  }
  if (IsShownOnScreen()) {
    Update();
  } else {
    m_forceOffscreenRender = true;
    InitGL();
    Render();
    m_forceOffscreenRender = false;
  }
}

void Viewer2DPanel::SetLayoutEditOverlay(std::optional<float> aspectRatio,
                                         std::optional<wxSize> viewportSize) {
  m_layoutEditAspect = aspectRatio;
  m_layoutEditViewportSize.reset();
  if (viewportSize && viewportSize->GetWidth() > 0 &&
      viewportSize->GetHeight() > 0) {
    m_layoutEditViewportSize = viewportSize;
    m_layoutEditBaseSize = viewportSize;
  } else {
    m_layoutEditBaseSize.reset();
  }
  m_layoutEditScale = 1.0f;
  RequestRepaint();
}

void Viewer2DPanel::SetLayoutEditOverlayScale(float scale) {
  if (!m_layoutEditAspect)
    return;
  m_layoutEditScale = std::clamp(scale, 0.1f, 10.0f);
  RequestRepaint();
}

// Sets the callback used to publish cursor and highlighted drag positions.
void Viewer2DPanel::SetCursorWorldPositionCallback(
    CursorWorldPositionCallback callback) {
  m_cursorWorldPositionCallback = std::move(callback);
}

void Viewer2DPanel::SetRenderOverrides(
    const std::optional<Viewer2DRenderOverrides> &overrides) {
  m_renderOverrides = overrides;
  RequestRepaint();
}

// Toggles the measurement tool mode and resets temporary measurement state.
void Viewer2DPanel::SetMeasureToolEnabled(bool enabled) {
  SetMeasureToolEnabled(enabled, m_measureToolState.mode);
}

// Toggles the measurement tool mode and applies the requested measuring mode.
void Viewer2DPanel::SetMeasureToolEnabled(bool enabled,
                                          Viewer2DMeasureMode mode) {
  m_measureToolState.enabled = enabled;
  m_measureToolState.mode = mode;
  ResetViewer2DMeasure(m_measureToolState);
  if (MainWindow::Instance())
    MainWindow::Instance()->SyncViewportToolToggleState(
        enabled, m_measureToolState.mode);
  SetCursor(enabled ? wxCursor(wxCURSOR_CROSS) : wxCursor(wxCURSOR_ARROW));
  RequestRepaint();
}

// Enables or disables Magnet snapping for selection dragging.
void Viewer2DPanel::SetMagnetEnabled(bool enabled, bool persist) {
  m_magnetEnabled = enabled;
  m_pendingMagnetSnap.reset();
  if (!persist)
    return;
  ConfigManager::Get().SetValue(magnet_snap::kMagnetEnabledConfigKey,
                                enabled ? "1" : "0");
  ConfigManager::Get().SaveUserConfig();
}

// Enables or disables left-click selection dragging.
void Viewer2DPanel::SetLeftDragSelectionMovementEnabled(bool enabled) {
  m_leftDragSelectionMovementEnabled = enabled;
}

// Enables or disables axis-constrained selection movement.
void Viewer2DPanel::SetAxisConstrainedMovementEnabled(bool enabled) {
  m_axisConstrainedMovementEnabled = enabled;
  if (!enabled)
    m_dragAxis = DragAxis::None;
}

// Sets whether axis-constrained viewport transforms use world or local axes.
void Viewer2DPanel::SetTransformSpace(transform_space::TransformSpace space) {
  m_transformSpace = space;
}

// Converts a mouse position in window coordinates into the current 2D world
// position.
std::optional<std::array<float, 3>>
Viewer2DPanel::ComputeWorldPositionFromScreen(const wxPoint &screenPos) const {
  const RenderSize renderSize =
      ResolveRenderSize(const_cast<Viewer2DPanel *>(this));
  if (!renderSize.IsValid())
    return std::nullopt;
  const int width = renderSize.width;
  const int height = renderSize.height;
  const wxPoint framebufferPos =
      ToFramebufferPoint(const_cast<Viewer2DPanel *>(this), screenPos);

  const float pixelsPerMeter = PIXELS_PER_METER * m_zoom;
  if (pixelsPerMeter <= 0.0f)
    return std::nullopt;

  const float offsetMetersX = m_offsetX / pixelsPerMeter;
  const float offsetMetersY = m_offsetY / pixelsPerMeter;

  const float viewX = (static_cast<float>(framebufferPos.x) -
                       static_cast<float>(width) * 0.5f) /
          pixelsPerMeter -
      offsetMetersX;
  const float viewY = (static_cast<float>(height) * 0.5f -
                       static_cast<float>(framebufferPos.y)) /
          pixelsPerMeter -
      offsetMetersY;

  switch (m_view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    return std::array<float, 3>{viewX, viewY, 0.0f};
  case Viewer2DView::Front:
    return std::array<float, 3>{viewX, 0.0f, viewY};
  case Viewer2DView::Side:
    return std::array<float, 3>{0.0f, viewX, viewY};
  }
  return std::array<float, 3>{viewX, viewY, 0.0f};
}

// Notifies listeners about the current cursor world position.
void Viewer2DPanel::NotifyCursorWorldPosition(const wxPoint &screenPos) {
  if (!m_cursorWorldPositionCallback)
    return;
  m_cursorWorldPositionCallback(ComputeWorldPositionFromScreen(screenPos),
                                false);
}

// Notifies listeners about a highlighted world position during active dragging.
void Viewer2DPanel::NotifyHighlightedWorldPosition(
    const std::optional<std::array<float, 3>> &positionMeters) {
  if (!m_cursorWorldPositionCallback)
    return;
  m_cursorWorldPositionCallback(positionMeters, true);
}

// Clears the cursor world position notification.
void Viewer2DPanel::ClearCursorWorldPosition() {
  if (!m_cursorWorldPositionCallback)
    return;
  m_cursorWorldPositionCallback(std::nullopt, false);
}

std::optional<wxSize> Viewer2DPanel::GetLayoutEditOverlaySize() const {
  if (!m_layoutEditAspect)
    return std::nullopt;
  wxSize baseSize(0, 0);
  if (m_layoutEditBaseSize) {
    baseSize = *m_layoutEditBaseSize;
  } else {
    int w = 0;
    int h = 0;
    const_cast<Viewer2DPanel *>(this)->GetClientSize(&w, &h);
    if (w > 0 && h > 0) {
      float aspect = *m_layoutEditAspect;
      float padding = static_cast<float>(std::min(w, h)) * 0.1f;
      float maxWidth = static_cast<float>(w) - padding * 2.0f;
      float maxHeight = static_cast<float>(h) - padding * 2.0f;
      float targetWidth = maxWidth;
      float targetHeight = targetWidth / aspect;
      if (targetHeight > maxHeight) {
        targetHeight = maxHeight;
        targetWidth = targetHeight * aspect;
      }
      baseSize = wxSize(static_cast<int>(std::lround(targetWidth)),
                        static_cast<int>(std::lround(targetHeight)));
    }
  }

  if (baseSize.GetWidth() <= 0 || baseSize.GetHeight() <= 0)
    return std::nullopt;

  int width =
      static_cast<int>(std::lround(baseSize.GetWidth() * m_layoutEditScale));
  int height =
      static_cast<int>(std::lround(baseSize.GetHeight() * m_layoutEditScale));
  return wxSize(width, height);
}

Viewer2DViewState Viewer2DPanel::GetViewState() const {
  int w = 0;
  int h = 0;
  const_cast<Viewer2DPanel *>(this)->GetClientSize(&w, &h);
  if (m_layoutEditViewportSize) {
    w = m_layoutEditViewportSize->GetWidth();
    h = m_layoutEditViewportSize->GetHeight();
  }

  Viewer2DViewState state{};
  state.offsetPixelsX = m_offsetX;
  state.offsetPixelsY = m_offsetY;
  state.zoom = m_zoom;
  state.viewportWidth = w;
  state.viewportHeight = h;
  state.view = m_view;
  return state;
}

std::shared_ptr<const SymbolDefinitionSnapshot>
Viewer2DPanel::GetBottomSymbolCacheSnapshot() const {
  return m_controller.GetBottomSymbolCacheSnapshot();
}

void Viewer2DPanel::InvalidateBottomSymbolCache() {
  m_controller.ClearBottomSymbolCache();
}

// Binds the OpenGL context needed for interaction hit-testing and selection
// readback.
bool Viewer2DPanel::TryBindGlContextForInteraction() {
  if (!m_glContext) {
    Logger::Instance().Log(Logger::Level::Warn,
                           "Viewer2DPanel: skipping interaction picking "
                           "because the OpenGL context is unavailable.");
    return false;
  }

  if (!IsShownOnScreen()) {
    Logger::Instance().Log(Logger::Level::Warn,
                           "Viewer2DPanel: skipping interaction picking "
                           "because the canvas is not shown.");
    return false;
  }

  if (!gl_lifecycle::TrySetCurrent(*this, m_glContext, "Viewer2DPanel",
                                    "interaction picking")) {
    Logger::Instance().Log(Logger::Level::Warn,
                           "Viewer2DPanel: interaction picking context-bind "
                           "failure; glContextBindFailed=true.");
    return false;
  }

  return true;
}

// Initializes the OpenGL context only when the canvas is safe to bind on this
// platform.
void Viewer2DPanel::InitGL() {
#if defined(__WXGTK__) || defined(__WXOSX__)
  if (!IsShownOnScreen()) {
    return;
  }
#else
  if (!IsShownOnScreen() && !m_forceOffscreenRender &&
      !m_allowOffscreenRender) {
    return;
  }
#endif
  if (!m_glInitialized) {
    const GLEWInitResult initResult =
        gl_lifecycle::InitializeGlew(*this, *m_glContext, "Viewer2DPanel");
    if (!initResult.success) {
      wxLogError("%s", initResult.message);
      return;
    }
    if (initResult.isWarningOnly) {
      wxLogDebug("%s", initResult.message);
    }

    m_controller.InitializeGL();
    glEnable(GL_DEPTH_TEST);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    m_glInitialized = true;
    return;
  }

  if (!gl_lifecycle::TrySetCurrent(*this, m_glContext, "Viewer2DPanel",
                                    "InitGL reuse")) {
    return;
  }
}

// Renders the 2D view to the onscreen back buffer and swaps buffers.
void Viewer2DPanel::Render() { RenderInternal(true); }

// Renders the full 2D scene using the active framebuffer-sized viewport.
void Viewer2DPanel::RenderInternal(bool swapBuffers) {
  static unsigned long long s_renderFrameId = 0;
  if (!m_glInitialized) {
    return;
  }
  // Interaction policy:
  // - Keep throttling expensive synchronization while recent interaction is
  //   active.
  // - Use interactive label mode only for visually expensive interaction
  //   paths. Simple selection clicks must not toggle it.
  const bool pauseHeavyTasks = m_enableSelection && ShouldPauseHeavyTasks();
  m_interactiveLabelMode =
      m_enableSelection && IsExpensiveVisualInteractionActive();
  RenderSize resolvedSize = ResolveRenderSize(this);
  if (m_captureFramebufferSizeOverride &&
      m_captureFramebufferSizeOverride->GetWidth() > 0 &&
      m_captureFramebufferSizeOverride->GetHeight() > 0) {
    resolvedSize =
        RenderSize{m_captureFramebufferSizeOverride->GetWidth(),
                   m_captureFramebufferSizeOverride->GetHeight(),
                   "RenderInternal(captureFramebufferSizeOverride-px)"};
  }
  const int w = resolvedSize.width;
  const int h = resolvedSize.height;
  if (!resolvedSize.IsValid())
    return;

  if (m_captureFramebufferSizeOverride) {
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, w, h);
  } else {
    glstate::ApplyKnownBaseOnscreenState(w, h);
  }
  const RenderSize viewportSize{w, h, "RenderInternal(active-framebuffer-px)"};

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float ppm = PIXELS_PER_METER * m_zoom;
  float halfW = static_cast<float>(w) / ppm * 0.5f;
  float halfH = static_cast<float>(h) / ppm * 0.5f;
  float offX = m_offsetX / PIXELS_PER_METER;
  float offY = m_offsetY / PIXELS_PER_METER;
  glOrtho(-halfW - offX, halfW - offX, -halfH - offY, halfH - offY, -100.0f,
          100.0f);
  const RenderSize projectionSize{
      w, h, "RenderInternal::world-projection(framebuffer-px)"};

  ++s_renderFrameId;
  ValidateRenderSizeContract("Viewer2DPanel", s_renderFrameId, resolvedSize,
                             viewportSize, projectionSize);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  switch (m_view) {
  case Viewer2DView::Top:
    gluLookAt(0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    break;
  case Viewer2DView::Bottom:
    gluLookAt(0.0, 0.0, -10.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    break;
  case Viewer2DView::Front:
    gluLookAt(0.0, -10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    break;
  case Viewer2DView::Side:
    gluLookAt(-10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    break;
  }

  ConfigManager &cfg = ConfigManager::Get();
  // Keep controller resources/cache in sync in the 2D viewer as well.
  // The 3D panel triggers this from its own render loop, but the 2D panel
  // renders directly through the shared controller and otherwise misses
  // scene/layer visibility refreshes.
  if (!pauseHeavyTasks)
    m_controller.UpdateResourcesIfDirty();
  bool darkMode = cfg.GetFloat("view2d_dark_mode") != 0.0f;
  if (m_renderOverrides && m_renderOverrides->darkMode.has_value())
    darkMode = m_renderOverrides->darkMode.value();
  m_controller.SetDarkMode(darkMode);
  if (darkMode)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  else
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bool showGrid = cfg.GetFloat("grid_show") != 0.0f;
  if (m_renderOverrides && m_renderOverrides->showGrid.has_value())
    showGrid = m_renderOverrides->showGrid.value();
  int gridStyle = static_cast<int>(cfg.GetFloat("grid_style"));
  float gridR = cfg.GetFloat("grid_color_r");
  float gridG = cfg.GetFloat("grid_color_g");
  float gridB = cfg.GetFloat("grid_color_b");
  bool drawAbove = cfg.GetFloat("grid_draw_above") != 0.0f;
  bool showRuler = cfg.GetFloat("ruler_show") != 0.0f;
  if (m_renderOverrides && m_renderOverrides->showRuler.has_value())
    showRuler = m_renderOverrides->showRuler.value();
  const float rulerSmallTickMeters = cfg.GetFloat("ruler_tick_small_m");
  const float rulerLargeTickMeters = cfg.GetFloat("ruler_tick_large_m");
  const float rulerAxisXPosition = cfg.GetFloat("ruler_axis_x_position");
  const float rulerAxisYPosition = cfg.GetFloat("ruler_axis_y_position");
  const float rulerAxisZPosition = cfg.GetFloat("ruler_axis_z_position");
  const float rulerAxisXColorR = cfg.GetFloat("ruler_axis_x_color_r");
  const float rulerAxisXColorG = cfg.GetFloat("ruler_axis_x_color_g");
  const float rulerAxisXColorB = cfg.GetFloat("ruler_axis_x_color_b");
  const float rulerAxisYColorR = cfg.GetFloat("ruler_axis_y_color_r");
  const float rulerAxisYColorG = cfg.GetFloat("ruler_axis_y_color_g");
  const float rulerAxisYColorB = cfg.GetFloat("ruler_axis_y_color_b");
  const float rulerAxisZColorR = cfg.GetFloat("ruler_axis_z_color_r");
  const float rulerAxisZColorG = cfg.GetFloat("ruler_axis_z_color_g");
  const float rulerAxisZColorB = cfg.GetFloat("ruler_axis_z_color_b");
  const auto distanceUnitSystem =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));

  std::optional<bool> forceBottomViewForTopFixturesOverride;
  std::optional<bool> symbolCaptureRenderProfileOverride;
  std::optional<bool> symbolCaptureIncludeCoplanarEdgesOverride;
  if (m_renderOverrides) {
    forceBottomViewForTopFixturesOverride =
        m_renderOverrides->forceBottomViewForTopFixtures;
    symbolCaptureRenderProfileOverride =
        m_renderOverrides->symbolCaptureRenderProfile;
    symbolCaptureIncludeCoplanarEdgesOverride =
        m_renderOverrides->symbolCaptureIncludeCoplanarEdges;
  }
  m_controller.SetForceBottomViewForTopFixturesOverride(
      forceBottomViewForTopFixturesOverride);
  m_controller.SetSymbolCaptureRenderProfileOverride(
      symbolCaptureRenderProfileOverride);
  m_controller.SetSymbolCaptureIncludeCoplanarEdgesOverride(
      symbolCaptureIncludeCoplanarEdgesOverride);

  std::unique_ptr<ICanvas2D> recordingCanvas;
  if (m_captureNextFrame) {
    m_lastCapturedFrame.Clear();
    recordingCanvas = CreateRecordingCanvas(m_lastCapturedFrame, false);
    // The recorded commands operate in the same world-space coordinates used by
    // the OpenGL renderer. We keep the transform identity so the exporter can
    // apply the viewport offsets/zoom exactly once using the captured
    // ViewState, matching the 2D on-screen projection.
    CanvasTransform transform{};
    transform.scale = 1.0f;
    transform.offsetX = 0.0f;
    transform.offsetY = 0.0f;
    recordingCanvas->BeginFrame();
    recordingCanvas->SetTransform(transform);
    m_controller.SetCaptureCanvas(recordingCanvas.get(), m_view,
                                  m_captureIncludeGrid,
                                  m_useSimplifiedFootprints);
  } else {
    m_controller.SetCaptureCanvas(nullptr, m_view);
  }

  m_controller.RenderScene(true, m_renderMode, m_view, showGrid, gridStyle,
                           gridR, gridG, gridB, drawAbove, true,
                           m_preferPerastageSvgSymbolsForLayouts);

  if (m_enableSelection && m_mouseInside && m_dragMode == DragMode::None) {
    if (!m_fastHoverHasPos || m_lastFastHoverScreenPos != m_lastMousePos) {
      m_lastFastHoverScreenPos = m_lastMousePos;
      m_fastHoverHasPos = true;
      const bool handledFastPath = TryUpdateHoverHighlightFast(m_lastMousePos);
      if (!handledFastPath)
        ScheduleHoverLabelRefresh(m_lastMousePos);
    }
  }

  if (m_layoutEditAspect && *m_layoutEditAspect > 0.0f) {
    if (!m_layoutEditBaseSize || m_layoutEditBaseSize->GetWidth() <= 0 ||
        m_layoutEditBaseSize->GetHeight() <= 0) {
      float aspect = *m_layoutEditAspect;
      float padding = static_cast<float>(std::min(w, h)) * 0.1f;
      float maxWidth = static_cast<float>(w) - padding * 2.0f;
      float maxHeight = static_cast<float>(h) - padding * 2.0f;
      float targetWidth = maxWidth;
      float targetHeight = targetWidth / aspect;
      if (targetHeight > maxHeight) {
        targetHeight = maxHeight;
        targetWidth = targetHeight * aspect;
      }
      m_layoutEditBaseSize =
          wxSize(static_cast<int>(std::lround(targetWidth)),
                 static_cast<int>(std::lround(targetHeight)));
    }

    if (m_layoutEditBaseSize && m_layoutEditBaseSize->GetWidth() > 0 &&
        m_layoutEditBaseSize->GetHeight() > 0) {
      float targetWidth = static_cast<float>(m_layoutEditBaseSize->GetWidth()) *
          m_layoutEditScale;
      float targetHeight =
          static_cast<float>(m_layoutEditBaseSize->GetHeight()) *
          m_layoutEditScale;
      float left = (static_cast<float>(w) - targetWidth) * 0.5f;
      float bottom = (static_cast<float>(h) - targetHeight) * 0.5f;

      GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
      if (depthEnabled)
        glDisable(GL_DEPTH_TEST);

      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      glOrtho(0.0f, static_cast<float>(w), 0.0f, static_cast<float>(h), -1.0f,
              1.0f);
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();

      glColor3f(1.0f, 0.0f, 0.0f);
      glLineWidth(2.0f);
      glBegin(GL_LINE_LOOP);
      glVertex2f(left, bottom);
      glVertex2f(left + targetWidth, bottom);
      glVertex2f(left + targetWidth, bottom + targetHeight);
      glVertex2f(left, bottom + targetHeight);
      glEnd();

      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);

      if (depthEnabled)
        glEnable(GL_DEPTH_TEST);
    }
  }

  if (showRuler) {
    viewer2d::RulerOverlayViewState rulerState;
    rulerState.width = w;
    rulerState.height = h;
    rulerState.zoom = m_zoom;
    rulerState.offsetPixelsX = m_offsetX;
    rulerState.offsetPixelsY = m_offsetY;
    rulerState.smallTickMeters = rulerSmallTickMeters;
    rulerState.largeTickMeters = rulerLargeTickMeters;
    rulerState.xRulerPositionMeters = rulerAxisXPosition;
    rulerState.yRulerPositionMeters = rulerAxisYPosition;
    rulerState.zRulerPositionMeters = rulerAxisZPosition;
    rulerState.xRulerColor = {rulerAxisXColorR, rulerAxisXColorG,
                              rulerAxisXColorB, 1.0f};
    rulerState.yRulerColor = {rulerAxisYColorR, rulerAxisYColorG,
                              rulerAxisYColorB, 1.0f};
    rulerState.zRulerColor = {rulerAxisZColorR, rulerAxisZColorG,
                              rulerAxisZColorB, 1.0f};
    rulerState.useImperialUnits =
        distanceUnitSystem == Units::DistanceUnitSystem::Imperial;
    rulerState.view = m_view;
    viewer2d::DrawRulerOverlay(rulerState, darkMode);
    const auto rulerLabels =
        viewer2d::BuildRulerScreenLabels(rulerState, darkMode);
    if (!rulerLabels.empty()) {
      std::vector<OverlayTextLabel> overlayLabels;
      overlayLabels.reserve(rulerLabels.size());
      for (const auto &label : rulerLabels) {
        overlayLabels.push_back({label.xPixels, label.yPixels, label.text,
                                 label.centerOnX, label.centerOnY,
                                 3.0f * m_zoom, true, label.color.r,
             label.color.g, label.color.b});
      }
      m_controller.DrawOverlayTextLabels(overlayLabels, darkMode);
    }
    if (recordingCanvas)
      viewer2d::EmitRulerToCanvas(rulerState, darkMode, *recordingCanvas);
  }

  if (m_dragMode == DragMode::Selection)
    DrawSelectionDragGizmo(w, h);

  if (m_measureToolState.enabled && m_measureToolState.hasAnchor) {
    std::optional<std::array<float, 3>> targetWorld;
    std::optional<std::array<float, 2>> targetScreenOverride;
    if (m_measureToolState.hasCommittedTarget) {
      targetWorld = m_measureToolState.committedTargetWorld;
    } else {
      targetWorld = ComputeWorldPositionFromScreen(m_lastMousePos);
      if (targetWorld) {
        // Keep live preview distance constrained to the active view plane.
        switch (m_view) {
        case Viewer2DView::Top:
        case Viewer2DView::Bottom:
          (*targetWorld)[2] = m_measureToolState.anchorMeasureWorld[2];
          break;
        case Viewer2DView::Front:
          (*targetWorld)[1] = m_measureToolState.anchorMeasureWorld[1];
          break;
        case Viewer2DView::Side:
          (*targetWorld)[0] = m_measureToolState.anchorMeasureWorld[0];
          break;
        }
      }
      const wxPoint framebufferMousePos =
          ToFramebufferPoint(this, m_lastMousePos);
      targetScreenOverride =
          std::array<float, 2>{static_cast<float>(framebufferMousePos.x),
          static_cast<float>(framebufferMousePos.y)};
    }
    if (targetWorld) {
      DrawMeasureOverlay(m_controller, m_measureToolState,
                         m_measureToolState.hasCommittedTarget
                             ? m_measureToolState.committedTargetMeasureWorld
                             : *targetWorld,
                         m_view, w, h, m_zoom, m_offsetX, m_offsetY,
                         distanceUnitSystem, darkMode, targetScreenOverride);
    }
  }

  // Draw fixture/hoist labels after all overlays so they remain on top of
  // rulers and other scene elements. Scale with zoom so labels behave like
  // regular scene objects instead of remaining a constant screen size.
  bool drawFixtureLabels = true;
  if (m_renderOverrides && m_renderOverrides->drawFixtureLabels.has_value())
    drawFixtureLabels = m_renderOverrides->drawFixtureLabels.value();
  if (drawFixtureLabels) {
    m_controller.DrawAllFixtureLabels(w, h, m_view, m_zoom,
                                      m_interactiveLabelMode);
  }

  if (swapBuffers && m_enableSelection && m_rectSelecting)
    DrawSelectionRectangle(w, h, darkMode);

  if (recordingCanvas) {
    recordingCanvas->EndFrame();
    m_captureNextFrame = false;
    m_controller.SetCaptureCanvas(nullptr, m_view);

    m_lastFixtureDebugReport.clear();
    auto debugKey =
        ConfigManager::Get().GetValue("print_viewer2d_fixture_debug_key");
    if (!debugKey || debugKey->empty()) {
      debugKey = ConfigManager::Get().GetValue("print_plan_fixture_debug_key");
    }
    if (debugKey && !debugKey->empty()) {
      m_lastFixtureDebugReport =
          BuildFixtureDebugReport(m_lastCapturedFrame, *debugKey);
        if (!m_lastFixtureDebugReport.empty()) {
          wxLogMessage("%s", wxString::FromUTF8(m_lastFixtureDebugReport));
        }
    }

    if (m_captureCallback) {
      // Capture buffer and state copies before invoking the callback to avoid
      // lifetime issues once the next frame is rendered.
      auto callback = std::move(m_captureCallback);
      CommandBuffer bufferCopy = m_lastCapturedFrame;
      Viewer2DViewState stateCopy = GetViewState();

      callback(std::move(bufferCopy), stateCopy);
    }
  }

  glFlush();
  if (!m_captureFramebufferSizeOverride) {
    ValidateGlStateAfterRender("Viewer2DPanel::RenderInternal", w, h);
  }
  if (swapBuffers)
    SwapBuffers();
}

// Captures the current 2D scene into an RGBA buffer using a framebuffer-sized
// viewport.
bool Viewer2DPanel::RenderToRGBA(
    std::vector<unsigned char> &pixels, int &width, int &height,
                                 const std::optional<wxSize> &targetFramebufferSize) {
  RenderSize renderSize = ResolveRenderSize(this);
  if (targetFramebufferSize && targetFramebufferSize->GetWidth() > 0 &&
      targetFramebufferSize->GetHeight() > 0) {
    renderSize = RenderSize{targetFramebufferSize->GetWidth(),
                            targetFramebufferSize->GetHeight(),
                            "RenderToRGBA(targetFramebufferSize-px)"};
  }
  const int w = renderSize.width;
  const int h = renderSize.height;
  if (w <= 0 || h <= 0) {
    diagnostics::DiagnosticReport::RecordViewer2DCaptureFailure(
        w, h, "Invalid capture target size");
    return false;
  }

  if (!TryAllocateCaptureBuffer(pixels, w, h)) {
    diagnostics::DiagnosticReport::RecordViewer2DCaptureFailure(
        w, h, "Unable to allocate capture buffer");
    return false;
  }
  width = w;
  height = h;

  struct ForceOffscreenGuard {
    Viewer2DPanel &panel;
    bool previous;

    // Restores the panel offscreen-render flag after capture.
    ~ForceOffscreenGuard() { panel.m_forceOffscreenRender = previous; }
  } forceGuard{*this, m_forceOffscreenRender};
  m_forceOffscreenRender = true;

  InitGL();
  if (!m_glInitialized) {
    diagnostics::DiagnosticReport::RecordViewer2DCaptureFailure(
        w, h, "OpenGL initialization failed");
    return false;
  }

  if (!m_captureFramebufferTargets)
    m_captureFramebufferTargets =
        std::make_unique<glcapture::FramebufferCaptureTargetCache>();

  glcapture::FramebufferCaptureTarget *target =
      m_captureFramebufferTargets->Acquire(w, h);
  ScopedReadBufferPackAlignmentState readStateGuard;
  glstate::ScopedFramebufferViewportScissorState framebufferStateGuard;
  if (!target || !target->IsComplete()) {
    const std::string diagnostic = m_captureFramebufferTargets->Diagnostic();
    diagnostics::DiagnosticReport::RecordViewer2DBackBufferFallback(
        w, h, diagnostic.empty() ? "FBO unavailable" : diagnostic);
    return RenderToRGBABackBufferFallback(pixels, w, h);
  }

  target->BindForRendering();
  {
    struct CaptureSizeOverrideGuard {
      Viewer2DPanel &panel;

      // Clears the temporary capture framebuffer size override after capture.
      ~CaptureSizeOverrideGuard() {
        panel.m_captureFramebufferSizeOverride.reset();
      }
    } captureSizeGuard{*this};

    m_captureFramebufferSizeOverride = wxSize(w, h);
    RenderInternal(false);
  }

  target->BindForReading();
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  diagnostics::DiagnosticReport::RecordViewer2DFboCapture(w, h);

  return true;
}

// Releases reusable capture framebuffer resources while the GL context is
// current.
void Viewer2DPanel::ReleaseCaptureFramebufferTarget() {
  if (m_captureFramebufferTargets)
    m_captureFramebufferTargets->Release();
}

// Captures RGBA pixels from the legacy back buffer path when FBO capture fails.
bool Viewer2DPanel::RenderToRGBABackBufferFallback(
    std::vector<unsigned char> &pixels, int width, int height) {
  struct CaptureSizeOverrideGuard {
    Viewer2DPanel &panel;

    // Clears the temporary capture framebuffer size override after fallback
    // capture.
    ~CaptureSizeOverrideGuard() {
      panel.m_captureFramebufferSizeOverride.reset();
    }
  } captureSizeGuard{*this};

  m_captureFramebufferSizeOverride = wxSize(width, height);
  RenderInternal(false);

  glReadBuffer(GL_BACK);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  return true;
}

// Handles paint events by refreshing interaction state and rendering the view.
void Viewer2DPanel::OnPaint(wxPaintEvent &WXUNUSED(event)) {
  wxPaintDC dc(this);
  ResetRepaintCoalescing();
  InitGL();

  const bool wasInteracting = m_isInteracting;
  const bool pauseHeavyTasks = ShouldPauseHeavyTasks();
  if (wasInteracting && !pauseHeavyTasks && m_dragMode == DragMode::None &&
      !m_rectSelecting) {
    m_controller.Update();
  }

  Render();
}

std::array<float, 3> Viewer2DPanel::MapDragDelta(float dxMeters,
                                                 float dyMeters) const {
  switch (m_view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    return {dxMeters, dyMeters, 0.0f};
  case Viewer2DView::Front:
    return {dxMeters, 0.0f, dyMeters};
  case Viewer2DView::Side:
    return {0.0f, dxMeters, dyMeters};
  }
  return {dxMeters, dyMeters, 0.0f};
}

// Computes the current center point for the dragged 2D selection.
std::optional<std::array<float, 3>>
Viewer2DPanel::ComputeSelectionDragCenterMeters() const {
  if (m_dragSelectionUuids.empty())
    return std::nullopt;

  const ConfigManager &cfg = ConfigManager::Get();
  std::array<float, 3> center{0.0f, 0.0f, 0.0f};
  size_t count = 0;
  for (const std::string &uuid : m_dragSelectionUuids) {
    const auto elementCenter = ResolveSceneElementCenterByUuid(cfg, uuid);
    if (!elementCenter)
      continue;
    center[0] += (*elementCenter)[0];
    center[1] += (*elementCenter)[1];
    center[2] += (*elementCenter)[2];
    ++count;
  }

  if (count == 0)
    return std::nullopt;
  center[0] /= static_cast<float>(count);
  center[1] /= static_cast<float>(count);
  center[2] /= static_cast<float>(count);
  return center;
}

// Draws a position gizmo at the active 2D selection drag center.
void Viewer2DPanel::DrawSelectionDragGizmo(int width, int height) {
  if (m_dragMode != DragMode::Selection || m_dragSelectionUuids.empty() ||
      width <= 0 || height <= 0)
    return;

  const auto centerWorld = ComputeSelectionDragCenterMeters();
  if (!centerWorld)
    return;

  const auto centerScreen = Viewer2DMeasureWorldToScreen(
      *centerWorld, m_view, width, height, m_zoom, m_offsetX, m_offsetY);
  if (!centerScreen)
    return;

  const bool depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height),
          -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  const float x = (*centerScreen)[0];
  // Viewer2DMeasureWorldToScreen returns top-origin pixels; this overlay uses
  // bottom-origin GL coordinates.
  const float y = static_cast<float>(height) - (*centerScreen)[1];
  const float length = 58.0f;
  const float arrowheadLength = 14.0f;
  const float arrowheadRadius = 6.0f;
  const float shaftLength = length - arrowheadLength;
  const bool horizontalActive = m_dragAxis == DragAxis::Horizontal;
  const bool verticalActive = m_dragAxis == DragAxis::Vertical;

  std::array<float, 4> horizontalColor{};
  std::array<float, 4> verticalColor{};
  switch (m_view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    horizontalColor = {horizontalActive ? 1.0f : 0.95f, 0.25f, 0.25f, 1.0f};
    verticalColor = {0.25f, verticalActive ? 1.0f : 0.95f, 0.25f, 1.0f};
    break;
  case Viewer2DView::Front:
    horizontalColor = {horizontalActive ? 1.0f : 0.95f, 0.25f, 0.25f, 1.0f};
    verticalColor = {0.25f, 0.45f, verticalActive ? 1.0f : 0.95f, 1.0f};
    break;
  case Viewer2DView::Side:
    horizontalColor = {0.25f, horizontalActive ? 1.0f : 0.95f, 0.25f, 1.0f};
    verticalColor = {0.25f, 0.45f, verticalActive ? 1.0f : 0.95f, 1.0f};
    break;
  }

  glLineWidth(2.5f);
  glBegin(GL_LINES);
  glColor4f(horizontalColor[0], horizontalColor[1], horizontalColor[2],
            horizontalColor[3]);
  glVertex2f(x, y);
  glVertex2f(x + shaftLength, y);
  glColor4f(verticalColor[0], verticalColor[1], verticalColor[2],
            verticalColor[3]);
  glVertex2f(x, y);
  glVertex2f(x, y + shaftLength);
  glEnd();

  glColor4f(horizontalColor[0], horizontalColor[1], horizontalColor[2],
            horizontalColor[3]);
  DrawSelectionDragArrowhead2D(x + shaftLength, y, 1.0f, 0.0f, arrowheadLength,
                               arrowheadRadius);
  glColor4f(verticalColor[0], verticalColor[1], verticalColor[2],
            verticalColor[3]);
  DrawSelectionDragArrowhead2D(x, y + shaftLength, 0.0f, 1.0f, arrowheadLength,
                               arrowheadRadius);

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

std::optional<magnet_snap::SnapSource>
Viewer2DPanel::BuildActiveMagnetSource() const {
  if (!m_magnetEnabled || m_measureToolState.enabled)
    return std::nullopt;
  if (!m_dragTrussUuids.empty() && m_dragSupportUuids.empty() &&
      m_dragSceneObjectUuids.empty()) {
    scene_grouping::ObjectSelection selection;
    selection.fixtures = m_dragFixtureUuids;
    selection.trusses = m_dragTrussUuids;
    const auto targets = scene_grouping::BuildInteractiveTransformTargets(
        ConfigManager::Get().GetScene(), selection,
        selection_movement_settings::LoadInteractiveTransformPolicy(
            ConfigManager::Get()));
    if (targets.size() == 1 && targets.front().type == MvrNodeType::GroupObject)
      return magnet_snap::SnapSource{magnet_snap::ObjectType::TrussGroup,
                                     targets.front().uuid};
    if (m_dragTrussUuids.size() == 1 && m_dragFixtureUuids.empty())
      return magnet_snap::SnapSource{magnet_snap::ObjectType::Truss,
                                     m_dragTrussUuids.front()};
  }
  if (m_dragFixtureUuids.size() == 1 && m_dragTrussUuids.empty() &&
      m_dragSupportUuids.empty() && m_dragSceneObjectUuids.empty())
    return magnet_snap::SnapSource{magnet_snap::ObjectType::Fixture,
                                   m_dragFixtureUuids.front()};
  if (m_dragSceneObjectUuids.size() == 1 && m_dragFixtureUuids.empty() &&
      m_dragTrussUuids.empty() && m_dragSupportUuids.empty())
    return magnet_snap::SnapSource{magnet_snap::ObjectType::SceneObject,
                                   m_dragSceneObjectUuids.front()};
  return std::nullopt;
}

// Builds view-aware Magnet settings for the active 2D projection.
magnet_snap::SnapSettings Viewer2DPanel::BuildActiveMagnetSettings() const {
  magnet_snap::SnapSettings settings;
  switch (m_view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    settings.axisWeights[2] = 0.0f;
    break;
  case Viewer2DView::Front:
    settings.axisWeights[1] = 0.0f;
    break;
  case Viewer2DView::Side:
    settings.axisWeights[0] = 0.0f;
    break;
  }
  return settings;
}

// Finds the current Magnet snap candidate for the active single-object drag.
std::optional<magnet_snap::SnapResult>
Viewer2DPanel::FindActiveMagnetSnap() const {
  auto source = BuildActiveMagnetSource();
  if (!source)
    return std::nullopt;
  return magnet_snap::FindSnap(ConfigManager::Get().GetScene(), *source,
                               BuildActiveMagnetSettings());
}

// Restores the raw mouse-following transform before applying the next drag
// delta.
std::optional<magnet_snap::SnapResult>
Viewer2DPanel::RestorePendingMagnetSnapPreview() {
  if (!m_pendingMagnetSnap)
    return std::nullopt;
  magnet_snap::SnapResult previous = *m_pendingMagnetSnap;
  magnet_snap::SnapResult inverse = previous;
  for (float &component : inverse.translationDeltaMm)
    component = -component;
  magnet_snap::ApplySnapTransform(
      ConfigManager::Get().GetScene(), inverse,
      selection_movement_settings::LoadInteractiveTransformPolicy(
          ConfigManager::Get()));
  m_pendingMagnetSnap.reset();
  return previous;
}

// Commits deferred Magnet grouping after a successful snap.
void Viewer2DPanel::CommitActiveMagnetSnap() {
  if (!m_pendingMagnetSnap || !m_pendingMagnetSnap->needsGrouping)
    return;
  ConfigManager &cfg = ConfigManager::Get();
  magnet_snap::ApplyCommittedSnapGrouping(cfg.GetScene(), *m_pendingMagnetSnap);
}

void Viewer2DPanel::ApplySelectionDelta(
    const std::array<float, 3> &deltaMeters) {
  if (m_dragSelectionUuids.empty())
    return;

  float dxMm = deltaMeters[0] * 1000.0f;
  float dyMm = deltaMeters[1] * 1000.0f;
  float dzMm = deltaMeters[2] * 1000.0f;
  if (dxMm == 0.0f && dyMm == 0.0f && dzMm == 0.0f)
    return;

  ConfigManager &cfg = ConfigManager::Get();
  if (!m_dragSelectionPushedUndo) {
    cfg.PushUndoState("move selection");
    m_dragSelectionPushedUndo = true;
  }
  std::lock_guard<std::mutex> sceneLock(m_dragTableUpdateSceneMutex);
  scene_grouping::ObjectSelection selection;
  selection.fixtures = m_dragFixtureUuids;
  selection.trusses = m_dragTrussUuids;
  selection.supports = m_dragSupportUuids;
  selection.sceneObjects = m_dragSceneObjectUuids;
  const auto previousSnap = RestorePendingMagnetSnapPreview();
  std::array<float, 3> deltaMm{dxMm, dyMm, dzMm};
  if (m_transformSpace == transform_space::TransformSpace::Local &&
      m_axisConstrainedMovementEnabled && m_dragAxis != DragAxis::None) {
    const auto targets = scene_grouping::BuildInteractiveTransformTargets(
        cfg.GetScene(), selection,
        selection_movement_settings::LoadInteractiveTransformPolicy(cfg));
    if (!targets.empty()) {
      const Matrix referenceTransform = scene_grouping::GetTargetWorldTransform(
          cfg.GetScene(), targets.front());
      deltaMm = transform_space::TransformDirection(
          transform_space::ExtractOrientation(referenceTransform), deltaMm);
    }
  }
  const auto policy =
      selection_movement_settings::LoadInteractiveTransformPolicy(cfg);
  scene_grouping::TranslateSelection(cfg.GetScene(), selection, deltaMm,
                                     transform_space::TransformSpace::World,
                                     policy);
  if (auto snap = FindActiveMagnetSnap()) {
    magnet_snap::ApplySnapTransform(cfg.GetScene(), *snap, policy);
    m_pendingMagnetSnap = snap;
  } else if (previousSnap) {
    magnet_snap::DetachSnapSourceFromGroup(cfg.GetScene(), *previousSnap);
  }
  NotifyHighlightedWorldPosition(ComputeSelectionDragCenterMeters());
  ScheduleDragTableUpdate();
}

void Viewer2DPanel::FinalizeSelectionDrag() {
  StopDragTableUpdates();
  ConfigManager &cfg = ConfigManager::Get();
  if (!m_dragFixtureUuids.empty() && FixtureTablePanel::Instance()) {
    auto selection = cfg.GetSelectedFixtures();
    FixtureTablePanel::Instance()->ReloadData();
    FixtureTablePanel::Instance()->SelectByUuid(selection, false);
  }
  if (!m_dragTrussUuids.empty() && TrussTablePanel::Instance()) {
    auto selection = cfg.GetSelectedTrusses();
    TrussTablePanel::Instance()->ReloadData();
    TrussTablePanel::Instance()->SelectByUuid(selection, false);
  }
  if (!m_dragSupportUuids.empty() && HoistTablePanel::Instance()) {
    auto selection = cfg.GetSelectedSupports();
    HoistTablePanel::Instance()->ReloadData();
    HoistTablePanel::Instance()->SelectByUuid(selection, false);
  }
  if (!m_dragSceneObjectUuids.empty() && SceneObjectTablePanel::Instance()) {
    auto selection = cfg.GetSelectedSceneObjects();
    SceneObjectTablePanel::Instance()->ReloadData();
    SceneObjectTablePanel::Instance()->SelectByUuid(selection, false);
  }

  if (!ShouldPauseHeavyTasks())
    UpdateScene(true);

  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  }
}

// Starts moving a newly created scene element with the pointer until it is
// placed.
void Viewer2DPanel::BeginContinuousPlacement(ContinuousPlacementType type,
                                             const std::string &elementUuid) {
  ConfigManager &cfg = ConfigManager::Get();
  if (!continuous_placement::Contains(cfg.GetScene(), type, elementUuid))
    return;

  m_continuousPlacementActive = true;
  m_continuousPlacementType = type;
  m_continuousPlacementNeedsPointerAlignment = true;
  m_continuousPlacementUuid = elementUuid;
  m_continuousPlacedUuids.clear();
  m_dragMode = DragMode::Selection;
  m_dragTarget = type == ContinuousPlacementType::Fixture ? DragTarget::Fixtures
                 : type == ContinuousPlacementType::Truss
                     ? DragTarget::Trusses
                     : DragTarget::SceneObjects;
  m_dragSelectionUuids = {elementUuid};
  m_dragFixtureUuids = type == ContinuousPlacementType::Fixture
                           ? std::vector<std::string>{elementUuid}
                           : std::vector<std::string>{};
  m_dragTrussUuids = type == ContinuousPlacementType::Truss
                         ? std::vector<std::string>{elementUuid}
                         : std::vector<std::string>{};
  m_dragSupportUuids.clear();
  m_dragSceneObjectUuids = type == ContinuousPlacementType::SceneObject
                               ? std::vector<std::string>{elementUuid}
                               : std::vector<std::string>{};
  m_dragSelectionMoved = false;
  m_dragSelectionPushedUndo = true;
  m_dragAxis = DragAxis::None;
  m_lastMousePos = ScreenToClient(wxGetMousePosition());
  m_pendingMagnetSnap.reset();
  SetFocus();
  RequestRepaint();
}

// Commits the current element and creates the next pointer-driven copy.
void Viewer2DPanel::ConfirmContinuousPlacement() {
  ConfigManager &cfg = ConfigManager::Get();
  if (!continuous_placement::Contains(cfg.GetScene(), m_continuousPlacementType,
                                      m_continuousPlacementUuid)) {
    CancelContinuousPlacement();
    return;
  }

  cfg.PushUndoState(std::string("place ") + continuous_placement::ElementName(
                        m_continuousPlacementType));
  m_continuousPlacedUuids.push_back(m_continuousPlacementUuid);
  const std::string nextUuid =
      wxString::Format(
          "uuid_%lld",
          static_cast<long long>(
              std::chrono::steady_clock::now().time_since_epoch().count()))
          .ToStdString();
  if (!continuous_placement::CloneElement(
          cfg.GetScene(), m_continuousPlacementType, m_continuousPlacementUuid,
          nextUuid)) {
    CancelContinuousPlacement();
    return;
  }
  CommitActiveMagnetSnap();
  const auto placedUuids = m_continuousPlacedUuids;
  BeginContinuousPlacement(m_continuousPlacementType, nextUuid);
  m_continuousPlacedUuids = placedUuids;
  m_continuousPlacementNeedsPointerAlignment = false;
  RefreshContinuousPlacementViews();
}

// Removes the uncommitted element and ends continuous placement.
void Viewer2DPanel::CancelContinuousPlacement() {
  RestorePendingMagnetSnapPreview();
  ConfigManager &cfg = ConfigManager::Get();
  continuous_placement::EraseElement(cfg.GetScene(), m_continuousPlacementType,
                                     m_continuousPlacementUuid);
  if (m_continuousPlacedUuids.empty()) {
    if (cfg.CanUndo())
      cfg.Undo();
  } else {
    const MvrScene finalScene = cfg.GetScene();
    for (size_t i = 0; i <= m_continuousPlacedUuids.size() && cfg.CanUndo();
         ++i) {
      cfg.Undo();
    }
    cfg.PushUndoState(
        std::string("continuous ") +
        continuous_placement::ElementName(m_continuousPlacementType) +
                      " placement");
    cfg.GetScene() = finalScene;
  }
  EndContinuousPlacementState();
  RefreshContinuousPlacementViews();
}

// Undoes one confirmed element while keeping the placement session active.
bool Viewer2DPanel::UndoContinuousPlacement() {
  if (!m_continuousPlacementActive)
    return false;

  ConfigManager &cfg = ConfigManager::Get();
  RestorePendingMagnetSnapPreview();
  if (m_continuousPlacedUuids.empty()) {
    if (cfg.CanUndo())
      cfg.Undo();
    EndContinuousPlacementState();
    RefreshContinuousPlacementViews();
    return true;
  }
  if (m_continuousPlacedUuids.size() == 1) {
    if (cfg.CanUndo())
      cfg.Undo();
    if (cfg.CanUndo())
      cfg.Undo();
    EndContinuousPlacementState();
    RefreshContinuousPlacementViews();
    return true;
  }

  const std::string restoredUuid = m_continuousPlacedUuids.back();
  m_continuousPlacedUuids.pop_back();
  if (cfg.CanUndo())
    cfg.Undo();
  if (!continuous_placement::Contains(cfg.GetScene(), m_continuousPlacementType,
                                      restoredUuid)) {
    EndContinuousPlacementState();
    RefreshContinuousPlacementViews();
    return true;
  }
  const auto placedUuids = m_continuousPlacedUuids;
  BeginContinuousPlacement(m_continuousPlacementType, restoredUuid);
  m_continuousPlacedUuids = placedUuids;
  RefreshContinuousPlacementViews();
  return true;
}

// Clears placement-only interaction state without changing the scene.
void Viewer2DPanel::EndContinuousPlacementState() {
  m_continuousPlacementActive = false;
  m_continuousPlacementType = ContinuousPlacementType::None;
  m_continuousPlacementNeedsPointerAlignment = false;
  m_continuousPlacementUuid.clear();
  m_continuousPlacedUuids.clear();
  m_dragMode = DragMode::None;
  m_dragTarget = DragTarget::None;
  m_dragSelectionUuids.clear();
  m_dragFixtureUuids.clear();
  m_dragTrussUuids.clear();
  m_dragSceneObjectUuids.clear();
  m_pendingMagnetSnap.reset();
  NotifyHighlightedWorldPosition(std::nullopt);
}

// Synchronizes tables and both viewers after a placement history change.
void Viewer2DPanel::RefreshContinuousPlacementViews() {
  if (MainWindow::Instance()) {
    MainWindow::Instance()->RefreshAfterToolSceneUpdate();
    return;
  }
  UpdateScene();
  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  }
  RequestRepaint();
}

// Selects scene items inside a dragged screen rectangle.
void Viewer2DPanel::ApplyRectangleSelection(const wxPoint &start,
                                            const wxPoint &end,
                                            bool selectAcrossAllTables,
                                            bool additive) {
  if (!m_enableSelection)
    return;
  if (!IsShownOnScreen())
    return;

  const RenderSize renderSize = ResolveRenderSize(this);
  const int w = renderSize.width;
  const int h = renderSize.height;
  if (w <= 0 || h <= 0)
    return;

  if (!TryBindGlContextForInteraction())
    return;

  const wxPoint pickStart = ToFramebufferPoint(this, start);
  const wxPoint pickEnd = ToFramebufferPoint(this, end);

  ConfigManager &cfg = ConfigManager::Get();
  if (selectAcrossAllTables) {
    auto fixtures = m_controller.GetFixturesInScreenRect(
        pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
    auto trusses = m_controller.GetTrussesInScreenRect(
        pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
    auto supports = Viewer2DSupportSelection::GetHoistsInScreenRect(
        pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h, cfg.GetScene(),
        cfg.GetHiddenLayers());
    auto sceneObjects = m_controller.GetSceneObjectsInScreenRect(
        pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
    if (additive) {
      fixtures = MergeSelectionAdditive(cfg.GetSelectedFixtures(), fixtures);
      trusses = MergeSelectionAdditive(cfg.GetSelectedTrusses(), trusses);
      supports = MergeSelectionAdditive(cfg.GetSelectedSupports(), supports);
      sceneObjects =
          MergeSelectionAdditive(cfg.GetSelectedSceneObjects(), sceneObjects);
    }

    const bool selectionChanged = fixtures != cfg.GetSelectedFixtures() ||
        trusses != cfg.GetSelectedTrusses() ||
        supports != cfg.GetSelectedSupports() ||
        sceneObjects != cfg.GetSelectedSceneObjects();
    if (selectionChanged)
      cfg.PushUndoState("global selection");

    cfg.SetSelectedFixtures(fixtures);
    cfg.SetSelectedTrusses(trusses);
    cfg.SetSelectedSupports(supports);
    cfg.SetSelectedSceneObjects(sceneObjects);

    std::set<std::string> mergedSelection;
    mergedSelection.insert(fixtures.begin(), fixtures.end());
    mergedSelection.insert(trusses.begin(), trusses.end());
    mergedSelection.insert(supports.begin(), supports.end());
    mergedSelection.insert(sceneObjects.begin(), sceneObjects.end());
    m_controller.SetSelectedUuids(std::vector<std::string>(
        mergedSelection.begin(), mergedSelection.end()));

    if (FixtureTablePanel::Instance()) {
      if (fixtures.empty())
        FixtureTablePanel::Instance()->ClearSelection();
      else
        FixtureTablePanel::Instance()->SelectByUuid(fixtures, false);
    }
    if (TrussTablePanel::Instance()) {
      if (trusses.empty())
        TrussTablePanel::Instance()->ClearSelection();
      else
        TrussTablePanel::Instance()->SelectByUuid(trusses, false);
    }
    if (HoistTablePanel::Instance()) {
      if (supports.empty())
        HoistTablePanel::Instance()->ClearSelection();
      else
        HoistTablePanel::Instance()->SelectByUuid(supports, false);
    }
    if (SceneObjectTablePanel::Instance()) {
      if (sceneObjects.empty())
        SceneObjectTablePanel::Instance()->ClearSelection();
      else
        SceneObjectTablePanel::Instance()->SelectByUuid(sceneObjects, false);
    }
    if (Viewer2DRenderPanel::Instance())
      Viewer2DRenderPanel::Instance()->RefreshLabelControlsFromSelection();
    return;
  }

  if (FixtureTablePanel::Instance() &&
      FixtureTablePanel::Instance()->IsActivePage()) {
    auto selection = m_controller.GetFixturesInScreenRect(
        pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
    if (additive)
      selection = MergeSelectionAdditive(cfg.GetSelectedFixtures(), selection);
    if (selection != cfg.GetSelectedFixtures()) {
      cfg.PushUndoState("fixture selection");
      cfg.SetSelectedFixtures(selection);
      if (Viewer2DRenderPanel::Instance())
        Viewer2DRenderPanel::Instance()->RefreshLabelControlsFromSelection();
    }
    m_controller.SetSelectedUuids(
        BuildViewerSelectionForTableSelection(cfg, selection, additive),
        additive ? BuildDirectSelection(cfg) : selection);
    if (selection.empty())
      FixtureTablePanel::Instance()->ClearSelection();
    else
      FixtureTablePanel::Instance()->SelectByUuid(selection, false);
  } else if (TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage()) {
    auto selection = m_controller.GetTrussesInScreenRect(
        pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
    if (additive)
      selection = MergeSelectionAdditive(cfg.GetSelectedTrusses(), selection);
    if (selection != cfg.GetSelectedTrusses()) {
      cfg.PushUndoState("truss selection");
      cfg.SetSelectedTrusses(selection);
    }
    m_controller.SetSelectedUuids(
        BuildViewerSelectionForTableSelection(cfg, selection, additive),
        additive ? BuildDirectSelection(cfg) : selection);
    if (selection.empty())
      TrussTablePanel::Instance()->ClearSelection();
    else
      TrussTablePanel::Instance()->SelectByUuid(selection, false);
  } else if (HoistTablePanel::Instance() &&
             HoistTablePanel::Instance()->IsActivePage()) {
    auto selection = Viewer2DSupportSelection::GetHoistsInScreenRect(
        pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h, cfg.GetScene(),
        cfg.GetHiddenLayers());
    if (additive)
      selection = MergeSelectionAdditive(cfg.GetSelectedSupports(), selection);
    if (selection != cfg.GetSelectedSupports()) {
      cfg.PushUndoState("support selection");
      cfg.SetSelectedSupports(selection);
    }
    m_controller.SetSelectedUuids(
        BuildViewerSelectionForTableSelection(cfg, selection, additive),
        additive ? BuildDirectSelection(cfg) : selection);
    if (selection.empty())
      HoistTablePanel::Instance()->ClearSelection();
    else
      HoistTablePanel::Instance()->SelectByUuid(selection, false);
  } else if (SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage()) {
    auto selection = m_controller.GetSceneObjectsInScreenRect(
        pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
    if (additive)
      selection =
          MergeSelectionAdditive(cfg.GetSelectedSceneObjects(), selection);
    if (selection != cfg.GetSelectedSceneObjects()) {
      cfg.PushUndoState("scene object selection");
      cfg.SetSelectedSceneObjects(selection);
    }
    m_controller.SetSelectedUuids(
        BuildViewerSelectionForTableSelection(cfg, selection, additive),
        additive ? BuildDirectSelection(cfg) : selection);
    if (selection.empty())
      SceneObjectTablePanel::Instance()->ClearSelection();
    else
      SceneObjectTablePanel::Instance()->SelectByUuid(selection, false);
  }
}

void Viewer2DPanel::DrawSelectionRectangle(int width, int height,
                                           bool darkMode) {
  if (!m_rectSelecting)
    return;

  int left = std::min(m_rectSelectStart.x, m_rectSelectEnd.x);
  int right = std::max(m_rectSelectStart.x, m_rectSelectEnd.x);
  int top = std::min(m_rectSelectStart.y, m_rectSelectEnd.y);
  int bottom = std::max(m_rectSelectStart.y, m_rectSelectEnd.y);
  const wxPoint physicalTopLeft = ToFramebufferPoint(this, wxPoint(left, top));
  const wxPoint physicalBottomRight =
      ToFramebufferPoint(this, wxPoint(right, bottom));

  float glLeft = static_cast<float>(physicalTopLeft.x);
  float glRight = static_cast<float>(physicalBottomRight.x);
  float glBottom = static_cast<float>(height - physicalBottomRight.y);
  float glTop = static_cast<float>(height - physicalTopLeft.y);

  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);

  GLboolean stippleEnabled = glIsEnabled(GL_LINE_STIPPLE);
  glEnable(GL_LINE_STIPPLE);
  glLineStipple(1, 0x00FF);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height),
          -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  if (darkMode)
    glColor3f(1.0f, 1.0f, 1.0f);
  else
    glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(1.5f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(glLeft, glBottom);
  glVertex2f(glRight, glBottom);
  glVertex2f(glRight, glTop);
  glVertex2f(glLeft, glTop);
  glEnd();

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  if (!stippleEnabled)
    glDisable(GL_LINE_STIPPLE);
  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

void Viewer2DPanel::ScheduleDragTableUpdate() {
  if (m_dragSelectionUuids.empty())
    return;
  if (ShouldPauseHeavyTasks())
    return;
  QueueDragTableUpdate(m_dragTarget, m_dragSelectionUuids);
}

void Viewer2DPanel::StopDragTableUpdates() {
  {
    std::lock_guard<std::mutex> lock(m_dragTableUpdateMutex);
    m_dragTableUpdateQueued = false;
    m_dragTableUpdateWorkerTarget = DragTarget::None;
    m_dragTableUpdateUuids.clear();
  }
}

void Viewer2DPanel::StartDragTableUpdateWorker() {
  m_dragTableUpdateWorker = std::thread([this]() {
    auto lastUpdate = std::chrono::steady_clock::time_point::min();
    const auto interval = std::chrono::milliseconds(kDragTableUpdateIntervalMs);
    while (true) {
      DragTarget target = DragTarget::None;
      std::vector<std::string> uuids;
      {
        std::unique_lock<std::mutex> lock(m_dragTableUpdateMutex);
        m_dragTableUpdateCv.wait(lock, [this]() {
          return m_dragTableWorkerStop || m_dragTableUpdateQueued;
        });
        if (m_dragTableWorkerStop)
          break;

        auto now = std::chrono::steady_clock::now();
        if (lastUpdate != std::chrono::steady_clock::time_point::min()) {
          auto elapsed = now - lastUpdate;
          if (elapsed < interval) {
            m_dragTableUpdateCv.wait_for(lock, interval - elapsed, [this]() {
              return m_dragTableWorkerStop;
            });
            if (m_dragTableWorkerStop)
              break;
          }
        }
        if (m_dragTableWorkerStop)
          break;
        target = m_dragTableUpdateWorkerTarget;
        uuids = std::move(m_dragTableUpdateUuids);
        m_dragTableUpdateQueued = false;
      }
      lastUpdate = std::chrono::steady_clock::now();

      if (uuids.empty())
        continue;

      auto snapshots = BuildDragTablePositionSnapshots(target, uuids);
      if (snapshots.empty())
        continue;

      std::vector<PositionValueUpdate> updates;
      updates.reserve(snapshots.size());
      for (const auto &snapshot : snapshots) {
        updates.push_back(PositionValueUpdate{
            snapshot.uuid, FormatMeters(snapshot.xMm),
            FormatMeters(snapshot.yMm), FormatMeters(snapshot.zMm)});
      }

      if (!wxTheApp)
        continue;

      wxTheApp->CallAfter([target, updates = std::move(updates)]() mutable {
        switch (target) {
        case DragTarget::Fixtures:
          if (FixtureTablePanel::Instance())
            FixtureTablePanel::Instance()->ApplyPositionValueUpdates(updates);
          break;
        case DragTarget::Trusses:
          if (TrussTablePanel::Instance())
            TrussTablePanel::Instance()->ApplyPositionValueUpdates(updates);
          break;
        case DragTarget::Supports:
          if (HoistTablePanel::Instance())
            HoistTablePanel::Instance()->ApplyPositionValueUpdates(updates);
          break;
        case DragTarget::SceneObjects:
          if (SceneObjectTablePanel::Instance())
            SceneObjectTablePanel::Instance()->ApplyPositionValueUpdates(
                updates);
          break;
        default:
          break;
        }
      });
    }
  });
}

void Viewer2DPanel::StopDragTableUpdateWorker() {
  {
    std::lock_guard<std::mutex> lock(m_dragTableUpdateMutex);
    m_dragTableWorkerStop = true;
    m_dragTableUpdateQueued = true;
  }
  m_dragTableUpdateCv.notify_one();
  if (m_dragTableUpdateWorker.joinable())
    m_dragTableUpdateWorker.join();
}

std::vector<Viewer2DPanel::DragTablePositionSnapshot>
Viewer2DPanel::BuildDragTablePositionSnapshots(
    DragTarget target, const std::vector<std::string> &uuids) {
  std::vector<DragTablePositionSnapshot> snapshots;
  snapshots.reserve(uuids.size());

  std::lock_guard<std::mutex> sceneLock(m_dragTableUpdateSceneMutex);
  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();

  switch (target) {
  case DragTarget::Fixtures:
    for (const auto &uuid : uuids) {
      auto it = scene.fixtures.find(uuid);
      if (it == scene.fixtures.end())
        continue;
      auto posArr = it->second.GetPosition();
      snapshots.push_back({uuid, posArr[0], posArr[1], posArr[2]});
    }
    break;
  case DragTarget::Trusses:
    for (const auto &uuid : uuids) {
      auto it = scene.trusses.find(uuid);
      if (it == scene.trusses.end())
        continue;
      auto posArr = it->second.transform.o;
      snapshots.push_back({uuid, posArr[0], posArr[1], posArr[2]});
    }
    break;
  case DragTarget::SceneObjects:
    for (const auto &uuid : uuids) {
      auto it = scene.sceneObjects.find(uuid);
      if (it == scene.sceneObjects.end())
        continue;
      auto posArr = it->second.transform.o;
      snapshots.push_back({uuid, posArr[0], posArr[1], posArr[2]});
    }
    break;
  case DragTarget::Supports:
    for (const auto &uuid : uuids) {
      auto it = scene.supports.find(uuid);
      if (it == scene.supports.end())
        continue;
      auto posArr = it->second.transform.o;
      snapshots.push_back({uuid, posArr[0], posArr[1], posArr[2]});
    }
    break;
  default:
    break;
  }

  return snapshots;
}

void Viewer2DPanel::QueueDragTableUpdate(DragTarget target,
                                         std::vector<std::string> uuids) {
  if (uuids.empty())
    return;

  {
    std::lock_guard<std::mutex> lock(m_dragTableUpdateMutex);
    m_dragTableUpdateWorkerTarget = target;
    m_dragTableUpdateUuids = std::move(uuids);
    m_dragTableUpdateQueued = true;
  }
  m_dragTableUpdateCv.notify_one();
}

bool Viewer2DPanel::ShouldPauseHeavyTasks() {
  // Returns true while recent input activity is still within the debounce
  // window. Callers should throttle only expensive synchronization work
  // (scene/resource updates, heavy snapshots). Visual overlays such as labels
  // should stay on-screen through an interactive lightweight rendering path.
  if (!m_isInteracting)
    return false;

  const auto now = std::chrono::steady_clock::now();
  if ((now - m_lastInteractionTime) < kPauseDelay)
    return true;

  m_isInteracting = false;
  return false;
}

bool Viewer2DPanel::IsExpensiveVisualInteractionActive() const {
  if (!m_enableSelection)
    return false;

  if (m_rectSelecting || m_dragMode == DragMode::RectSelection)
    return true;

  if (m_dragMode == DragMode::View)
    return true;

  return m_dragMode == DragMode::Selection && m_dragSelectionMoved;
}

void Viewer2DPanel::MarkInteractionActivity() {
  m_isInteracting = true;
  m_lastInteractionTime = std::chrono::steady_clock::now();
  m_interactionResumeTimer.StartOnce(static_cast<int>(kPauseDelay.count()) +
                                     10);
}

void Viewer2DPanel::OnInteractionPauseTimer(wxTimerEvent &WXUNUSED(event)) {
  if (!ShouldPauseHeavyTasks())
    RequestRepaint();
}

void Viewer2DPanel::OnHoverHitTestTimer(wxTimerEvent &WXUNUSED(event)) {
  if (!m_hoverHitTestPending)
    return;
  RunHoverHitTest(m_pendingHoverScreenPos);
}

void Viewer2DPanel::ScheduleHoverLabelRefresh(const wxPoint &screenPos) {
  ScheduleHoverHitTest(screenPos, false);
}

int Viewer2DPanel::GetHoverHitTestIntervalMs() const {
  const bool isDragging = m_dragMode != DragMode::None;
  const bool interacting = m_isInteracting || m_viewMotionSinceLastHoverHitTest;
  if (isDragging || interacting)
    return kHoverHitTestInteractingIntervalMs;
  return kHoverHitTestIdleIntervalMs;
}

int Viewer2DPanel::GetHoverMoveThresholdPx() const {
  if (m_dragMode != DragMode::None)
    return kHoverMoveThresholdPx;
  return kHoverIdleMoveThresholdPx;
}

void Viewer2DPanel::ScheduleHoverHitTest(const wxPoint &screenPos,
                                         bool forceNow) {
  if (!m_enableSelection || !IsShownOnScreen())
    return;

  m_pendingHoverScreenPos = screenPos;
  m_hoverHitTestPending = true;
  const int hitTestIntervalMs = GetHoverHitTestIntervalMs();
  const int moveThresholdPx = GetHoverMoveThresholdPx();

  const auto now = std::chrono::steady_clock::now();
  if (!forceNow && m_hoverQueryHasPos) {
    const int manhattanMoved =
        std::abs(screenPos.x - m_lastHoverQueryScreenPos.x) +
        std::abs(screenPos.y - m_lastHoverQueryScreenPos.y);
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastHoverHitTestTime)
            .count();
    if (manhattanMoved < moveThresholdPx && elapsedMs < hitTestIntervalMs)
      return;
  }

  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now - m_lastHoverHitTestTime)
          .count();
  if (forceNow || elapsedMs >= hitTestIntervalMs) {
    m_hoverHitTestTimer.Stop();
    RunHoverHitTest(screenPos);
    return;
  }

  const int delayMs =
      std::max(1, hitTestIntervalMs - static_cast<int>(elapsedMs));
  m_hoverHitTestTimer.StartOnce(delayMs);
}

bool Viewer2DPanel::ApplyHoverUuid(const std::string &newUuid,
                                   bool requestRepaint) {
  const std::string oldHoverUuid = m_hoverUuid;
  if (oldHoverUuid == newUuid && m_hasHover == !newUuid.empty())
    return false;

  m_hasHover = !newUuid.empty();
  m_hoverUuid = newUuid;
  m_controller.SetHighlightUuid(m_hoverUuid);

  if (FixtureTablePanel::Instance()) {
    const std::string value = FixtureTablePanel::Instance()->IsActivePage()
                                  ? m_hoverUuid
                                  : std::string();
    FixtureTablePanel::Instance()->HighlightFixture(value);
  }
  if (TrussTablePanel::Instance()) {
    const std::string value = TrussTablePanel::Instance()->IsActivePage()
                                  ? m_hoverUuid
                                  : std::string();
    TrussTablePanel::Instance()->HighlightTruss(value);
  }
  if (HoistTablePanel::Instance()) {
    const std::string value = HoistTablePanel::Instance()->IsActivePage()
                                  ? m_hoverUuid
                                  : std::string();
    HoistTablePanel::Instance()->HighlightHoist(value);
  }
  if (SceneObjectTablePanel::Instance()) {
    const std::string value = SceneObjectTablePanel::Instance()->IsActivePage()
                                  ? m_hoverUuid
                                  : std::string();
    SceneObjectTablePanel::Instance()->HighlightObject(value);
  }

  if (requestRepaint)
    RequestRepaint();
  return true;
}

void Viewer2DPanel::ClearHoverState(bool requestRepaint) {
  ApplyHoverUuid("", requestRepaint);
}

// Invalidates cached picking results after scene, view, or visibility changes.
void Viewer2DPanel::InvalidatePickCache() {
  m_pickCache.valid = false;
  ++m_pickCacheSceneGeneration;
}

// Builds a hash for hidden-layer state and invalidates stale pick cache
// entries.
size_t Viewer2DPanel::BuildHiddenLayersHash() {
  const size_t hash =
      HashStringContainer(ConfigManager::Get().GetHiddenLayers());
  if (m_pickCache.valid && m_pickCache.hiddenLayersHash != hash)
    InvalidatePickCache();
  return hash;
}

// Returns whether a recent picking query can be reused for the current pointer
// location.
bool Viewer2DPanel::IsPickCacheReusable(PickQueryKind queryKind,
                                        const wxPoint &framebufferPos,
                                        int viewportWidth, int viewportHeight,
                                        size_t hiddenLayersHash,
                                        bool clickSelection) const {
  if (!m_pickCache.valid || m_pickCache.queryKind != queryKind)
    return false;
  if (m_pickCache.viewportWidth != viewportWidth ||
      m_pickCache.viewportHeight != viewportHeight)
    return false;
  if (m_pickCache.view != m_view)
    return false;
  if (m_pickCache.hiddenLayersHash != hiddenLayersHash)
    return false;
  if (m_pickCache.clickSelection != clickSelection)
    return false;
  if (m_pickCache.sceneGeneration != m_pickCacheSceneGeneration)
    return false;
  const int dx = framebufferPos.x - m_pickCache.framebufferPos.x;
  const int dy = framebufferPos.y - m_pickCache.framebufferPos.y;
  return (dx * dx + dy * dy) <=
         (kPickCacheReuseDistancePx * kPickCacheReuseDistancePx);
}

// Stores a pick result and logs the first interaction pick after a scene
// update.
void Viewer2DPanel::StorePickCache(PickQueryKind queryKind,
                                   const wxPoint &framebufferPos,
                                   int viewportWidth, int viewportHeight,
                                   size_t hiddenLayersHash, bool clickSelection,
                                   bool found, const std::string &uuid) {
  m_pickCache.valid = true;
  m_pickCache.queryKind = queryKind;
  m_pickCache.framebufferPos = framebufferPos;
  m_pickCache.viewportWidth = viewportWidth;
  m_pickCache.viewportHeight = viewportHeight;
  m_pickCache.view = m_view;
  m_pickCache.hiddenLayersHash = hiddenLayersHash;
  m_pickCache.clickSelection = clickSelection;
  m_pickCache.sceneGeneration = m_pickCacheSceneGeneration;
  m_pickCache.timestamp = std::chrono::steady_clock::now();
  m_pickCache.found = found;
  m_pickCache.uuid = uuid;

  if (m_logFirstPickAfterSceneUpdate) {
    const char *queryName = "none";
    switch (queryKind) {
    case PickQueryKind::FixtureLabel:
      queryName = "fixture-label";
      break;
    case PickQueryKind::TrussLabel:
      queryName = "truss-label";
      break;
    case PickQueryKind::HoistLabel:
      queryName = "hoist-label";
      break;
    case PickQueryKind::SceneObjectLabel:
      queryName = "scene-object-label";
      break;
    case PickQueryKind::PickUuid:
      queryName = "pick-uuid";
      break;
    case PickQueryKind::None:
      break;
    }
    // This diagnostic only reports cursor hit-testing after a scene refresh.
    Logger::Instance().Log(
        Logger::Level::Debug,
        "Viewer2DPanel: first cursor hit-test after scene update; "
        "reloadRequested=" +
            std::string(m_lastUpdateSceneReloadRequested ? "true" : "false") +
            " query='" + queryName +
            "' objectUnderCursor=" + std::string(found ? "true" : "false") +
            " sceneGeneration=" + std::to_string(m_pickCacheSceneGeneration) +
            ".");
    m_logFirstPickAfterSceneUpdate = false;
  }
}

// Resolves a picked UUID using the reusable interaction pick cache.
bool Viewer2DPanel::TryResolvePickUuidWithCache(const wxPoint &framebufferPos,
                                                int viewportWidth,
                                                int viewportHeight,
                                                size_t hiddenLayersHash,
                                                std::string &uuidOut) {
  if (IsPickCacheReusable(PickQueryKind::PickUuid, framebufferPos,
                          viewportWidth, viewportHeight, hiddenLayersHash,
                          false)) {
    uuidOut = m_pickCache.uuid;
    return m_pickCache.found;
  }

  std::string pickedUuid;
  const bool found = m_controller.GetPickUuidAt(
      framebufferPos.x, framebufferPos.y, viewportWidth, viewportHeight,
      ConfigManager::Get().GetHiddenLayers(), pickedUuid);
  StorePickCache(PickQueryKind::PickUuid, framebufferPos, viewportWidth,
                 viewportHeight, hiddenLayersHash, false, found, pickedUuid);
  if (!found)
    uuidOut.clear();
  else
    uuidOut = pickedUuid;
  return found;
}

// Resolves a picked label using the reusable interaction pick cache.
bool Viewer2DPanel::TryResolvePickLabelWithCache(
    PickQueryKind queryKind, const wxPoint &framebufferPos, int viewportWidth,
    int viewportHeight, size_t hiddenLayersHash, bool clickSelection,
                                                 std::string &uuidOut) {
  if (IsPickCacheReusable(queryKind, framebufferPos, viewportWidth,
                          viewportHeight, hiddenLayersHash, clickSelection)) {
    uuidOut = m_pickCache.uuid;
    return m_pickCache.found;
  }

  wxString label;
  wxPoint pos;
  std::string pickedUuid;
  bool found = false;
  switch (queryKind) {
  case PickQueryKind::FixtureLabel:
    found = m_controller.GetFixtureLabelAt(framebufferPos.x, framebufferPos.y,
                                           viewportWidth, viewportHeight, label,
                                           pos, &pickedUuid, clickSelection);
    break;
  case PickQueryKind::TrussLabel:
    found = m_controller.GetTrussLabelAt(framebufferPos.x, framebufferPos.y,
                                         viewportWidth, viewportHeight, label,
                                         pos, &pickedUuid, clickSelection);
    break;
  case PickQueryKind::HoistLabel:
    found = Viewer2DSupportSelection::FindHoistAtScreenPoint(
        framebufferPos.x, framebufferPos.y, viewportHeight,
        ConfigManager::Get().GetScene(), ConfigManager::Get().GetHiddenLayers(),
        pickedUuid, pos, label);
    break;
  case PickQueryKind::SceneObjectLabel:
    found = m_controller.GetSceneObjectLabelAt(
        framebufferPos.x, framebufferPos.y, viewportWidth, viewportHeight,
        label, pos, &pickedUuid, clickSelection);
    break;
  case PickQueryKind::None:
  case PickQueryKind::PickUuid:
    break;
  }

  StorePickCache(queryKind, framebufferPos, viewportWidth, viewportHeight,
                 hiddenLayersHash, clickSelection, found, pickedUuid);
  if (!found)
    uuidOut.clear();
  else
    uuidOut = pickedUuid;
  return found;
}

// Updates hover highlighting through the lightweight pick-UUID path when
// available.
bool Viewer2DPanel::TryUpdateHoverHighlightFast(const wxPoint &screenPos) {
  if (!m_enableSelection || !IsShownOnScreen() || m_dragMode != DragMode::None)
    return false;

  const bool crossTableActions = IsCrossTableViewportActionsEnabled();
  const bool fixtureActive = FixtureTablePanel::Instance() &&
                             FixtureTablePanel::Instance()->IsActivePage();
  const bool trussActive = TrussTablePanel::Instance() &&
                           TrussTablePanel::Instance()->IsActivePage();
  const bool sceneObjectActive =
      SceneObjectTablePanel::Instance() &&
                                 SceneObjectTablePanel::Instance()->IsActivePage();
  const bool hoistActive = HoistTablePanel::Instance() &&
                           HoistTablePanel::Instance()->IsActivePage();
  if (!crossTableActions &&
      (hoistActive || (!fixtureActive && !trussActive && !sceneObjectActive)))
    return false;

  const RenderSize renderSize = ResolveRenderSize(this);
  if (!renderSize.IsValid())
    return false;
  if (!TryBindGlContextForInteraction())
    return false;

  const wxPoint pickPos = ToFramebufferPoint(this, screenPos);
  const size_t hiddenLayersHash = BuildHiddenLayersHash();
  std::string pickedUuid;
  if (!TryResolvePickUuidWithCache(pickPos, renderSize.width, renderSize.height,
                                   hiddenLayersHash, pickedUuid)) {
    const bool changed = ApplyHoverUuid("", true);
    if (changed)
      ScheduleHoverLabelRefresh(screenPos);
    return true;
  }

  const auto &scene = ConfigManager::Get().GetScene();
  std::string newUuid;
  if ((crossTableActions || fixtureActive) &&
      scene.fixtures.find(pickedUuid) != scene.fixtures.end())
    newUuid = pickedUuid;
  else if ((crossTableActions || trussActive) &&
           scene.trusses.find(pickedUuid) != scene.trusses.end())
    newUuid = pickedUuid;
  else if ((crossTableActions || hoistActive) &&
           scene.supports.find(pickedUuid) != scene.supports.end())
    newUuid = pickedUuid;
  else if ((crossTableActions || sceneObjectActive) &&
           scene.sceneObjects.find(pickedUuid) != scene.sceneObjects.end())
    newUuid = pickedUuid;

  const bool changed = ApplyHoverUuid(newUuid, true);
  if (changed)
    ScheduleHoverLabelRefresh(screenPos);
  return true;
}

// Resolves the hovered scene item and updates hover highlighting.
void Viewer2DPanel::RunHoverHitTest(const wxPoint &screenPos) {
  const auto queryStartTime = std::chrono::steady_clock::now();
  if (!m_enableSelection || !IsShownOnScreen())
    return;

  m_hoverHitTestPending = false;
  if (m_dragMode == DragMode::Selection)
    return;
  m_lastHoverHitTestTime = std::chrono::steady_clock::now();
  m_lastHoverQueryScreenPos = screenPos;
  m_hoverQueryHasPos = true;
  m_viewMotionSinceLastHoverHitTest = false;

  const bool skipLabelWork = ShouldPauseHeavyTasks();
  if (skipLabelWork) {
    ClearHoverState(true);
    return;
  }

  const RenderSize renderSize = ResolveRenderSize(this);
  const int w = renderSize.width;
  const int h = renderSize.height;
  if (!renderSize.IsValid())
    return;
  if (!TryBindGlContextForInteraction())
    return;

  const wxPoint pickPos = ToFramebufferPoint(this, screenPos);
  const size_t hiddenLayersHash = BuildHiddenLayersHash();
  std::string newUuid;
  bool found = false;

  if (IsCrossTableViewportActionsEnabled()) {
    found =
        TryResolvePickUuidWithCache(pickPos, w, h, hiddenLayersHash, newUuid);
  } else if (FixtureTablePanel::Instance() &&
             FixtureTablePanel::Instance()->IsActivePage()) {
    found =
        TryResolvePickLabelWithCache(PickQueryKind::FixtureLabel, pickPos, w, h,
                                         hiddenLayersHash, false, newUuid);
    if (found) {
      if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->HighlightTruss(std::string());
      if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    }
  } else if (TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage()) {
    found = TryResolvePickLabelWithCache(PickQueryKind::TrussLabel, pickPos, w,
                                         h, hiddenLayersHash, false, newUuid);
    if (found) {
      if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->HighlightFixture(std::string());
      if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    }
  } else if (HoistTablePanel::Instance() &&
             HoistTablePanel::Instance()->IsActivePage()) {
    found = TryResolvePickLabelWithCache(PickQueryKind::HoistLabel, pickPos, w,
                                         h, hiddenLayersHash, false, newUuid);
    if (found) {
      if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->HighlightFixture(std::string());
      if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->HighlightTruss(std::string());
      if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    }
  } else if (SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage()) {
    found =
        TryResolvePickLabelWithCache(PickQueryKind::SceneObjectLabel, pickPos,
                                         w, h, hiddenLayersHash, false, newUuid);
    if (found) {
      if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->HighlightFixture(std::string());
      if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->HighlightTruss(std::string());
    }
  }

  if (found) {
    ApplyHoverUuid(newUuid, true);
  } else {
    ApplyHoverUuid("", true);
  }

  const auto resolveDuration =
      std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - queryStartTime);
  TrackHoverHitTestTelemetry(resolveDuration);
}

void Viewer2DPanel::TrackHoverHitTestTelemetry(
    std::chrono::microseconds duration) {
#ifndef NDEBUG
  const auto now = std::chrono::steady_clock::now();
  if (m_hoverTelemetryWindowStart.time_since_epoch().count() == 0)
    m_hoverTelemetryWindowStart = now;
  ++m_hoverQueriesInCurrentWindow;
  m_hoverTotalResolveTimeInCurrentWindow += duration;
  const auto elapsed = now - m_hoverTelemetryWindowStart;
  if (elapsed >= std::chrono::seconds(1)) {
    const double avgResolveMs =
        m_hoverQueriesInCurrentWindow > 0
            ? static_cast<double>(
                  m_hoverTotalResolveTimeInCurrentWindow.count()) /
                  static_cast<double>(m_hoverQueriesInCurrentWindow) / 1000.0
            : 0.0;
    wxLogDebug("Viewer2DPanel hover queries/s: %d avg resolve: %.2f ms",
               m_hoverQueriesInCurrentWindow, avgResolveMs);
    m_hoverTelemetryWindowStart = now;
    m_hoverQueriesInCurrentWindow = 0;
    m_hoverTotalResolveTimeInCurrentWindow = std::chrono::microseconds{0};
  }
#else
  (void)duration;
#endif
}

// Handles left-button press setup for view dragging, selection dragging, and
// rectangle selection.
void Viewer2DPanel::OnMouseDown(wxMouseEvent &event) {
  if (m_continuousPlacementActive && event.LeftDown()) {
    CaptureMouse();
    m_draggedSincePress = false;
    m_dragMode = DragMode::View;
    m_lastMousePos = event.GetPosition();
    MarkInteractionActivity();
    return;
  }
  if (event.LeftDown()) {
    CaptureMouse();
    m_draggedSincePress = false;
    m_dragPressTime = wxGetLocalTimeMillis();
    m_dragMode = DragMode::View;
    m_dragAxis = DragAxis::None;
    m_dragTarget = DragTarget::None;
    m_dragSelectionUuids.clear();
    m_dragFixtureUuids.clear();
    m_dragTrussUuids.clear();
    m_dragSupportUuids.clear();
    m_dragSceneObjectUuids.clear();
    m_dragSelectionMoved = false;
    m_dragSelectionPushedUndo = false;
    m_rectSelectionAcrossAllTables = false;
    m_lastMousePos = event.GetPosition();
    MarkInteractionActivity();
    m_hoverHitTestTimer.Stop();
    m_hoverHitTestPending = false;

    if (!m_enableSelection || !IsShownOnScreen())
      return;

    if (event.ControlDown()) {
      m_dragMode = DragMode::RectSelection;
      m_rectSelecting = true;
      m_rectSelectionAcrossAllTables =
          event.ShiftDown() || IsCrossTableViewportActionsEnabled();
      m_rectSelectStart = m_lastMousePos;
      m_rectSelectEnd = m_lastMousePos;
      return;
    }

    if (!m_leftDragSelectionMovementEnabled)
      return;

    const RenderSize renderSize = ResolveRenderSize(this);
    const int w = renderSize.width;
    const int h = renderSize.height;
    if (w <= 0 || h <= 0)
      return;
    if (!TryBindGlContextForInteraction())
      return;

    std::string uuid;
    const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
    const size_t hiddenLayersHash = BuildHiddenLayersHash();
    bool found = false;
    DragTarget target = DragTarget::None;
    if (FixtureTablePanel::Instance() &&
        FixtureTablePanel::Instance()->IsActivePage()) {
      found = TryResolvePickLabelWithCache(PickQueryKind::FixtureLabel, pickPos,
                                           w, h, hiddenLayersHash, false, uuid);
      target = DragTarget::Fixtures;
    } else if (TrussTablePanel::Instance() &&
               TrussTablePanel::Instance()->IsActivePage()) {
      found = TryResolvePickLabelWithCache(PickQueryKind::TrussLabel, pickPos,
                                           w, h, hiddenLayersHash, false, uuid);
      target = DragTarget::Trusses;
    } else if (HoistTablePanel::Instance() &&
               HoistTablePanel::Instance()->IsActivePage()) {
      found = TryResolvePickLabelWithCache(PickQueryKind::HoistLabel, pickPos,
                                           w, h, hiddenLayersHash, false, uuid);
      target = DragTarget::Supports;
    } else if (SceneObjectTablePanel::Instance() &&
               SceneObjectTablePanel::Instance()->IsActivePage()) {
      found =
          TryResolvePickLabelWithCache(PickQueryKind::SceneObjectLabel, pickPos,
                                           w, h, hiddenLayersHash, false, uuid);
      target = DragTarget::SceneObjects;
    }

    if (found && target != DragTarget::None) {
      ConfigManager &cfg = ConfigManager::Get();
      std::vector<std::string> selection;
      switch (target) {
      case DragTarget::Fixtures:
        selection = cfg.GetSelectedFixtures();
        break;
      case DragTarget::Trusses:
        selection = cfg.GetSelectedTrusses();
        break;
      case DragTarget::Supports:
        selection = cfg.GetSelectedSupports();
        break;
      case DragTarget::SceneObjects:
        selection = cfg.GetSelectedSceneObjects();
        break;
      default:
        break;
      }

      auto it = std::find(selection.begin(), selection.end(), uuid);
      const bool dragCurrentSelection = it != selection.end();
      if (selection.size() > 1 || dragCurrentSelection)
        m_dragSelectionUuids = selection;
      else
        m_dragSelectionUuids = {uuid};

      if (dragCurrentSelection) {
        m_dragFixtureUuids = cfg.GetSelectedFixtures();
        m_dragTrussUuids = cfg.GetSelectedTrusses();
        m_dragSupportUuids = cfg.GetSelectedSupports();
        m_dragSceneObjectUuids = cfg.GetSelectedSceneObjects();
      } else {
        m_dragFixtureUuids.clear();
        m_dragTrussUuids.clear();
        m_dragSupportUuids.clear();
        m_dragSceneObjectUuids.clear();
        switch (target) {
        case DragTarget::Fixtures:
          m_dragFixtureUuids = {uuid};
          break;
        case DragTarget::Trusses:
          m_dragTrussUuids = {uuid};
          break;
        case DragTarget::Supports:
          m_dragSupportUuids = {uuid};
          break;
        case DragTarget::SceneObjects:
          m_dragSceneObjectUuids = {uuid};
          break;
        default:
          break;
        }
      }

      ApplyHoverUuid(uuid, true);
      m_dragMode = DragMode::Selection;
      m_dragTarget = target;
    }
  }
}

// Opens the editor for the double-clicked fixture or scene object.
void Viewer2DPanel::OnMouseDClick(wxMouseEvent &event) {
  const RenderSize renderSize = ResolveRenderSize(this);
  const int w = renderSize.width;
  const int h = renderSize.height;
  if (w <= 0 || h <= 0 || !IsShownOnScreen())
    return;
  if (!TryBindGlContextForInteraction())
    return;

  wxString label;
  wxPoint pos;
  std::string uuid;
  const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
  ConfigManager &cfg = ConfigManager::Get();

  const bool sceneObjectsActive =
      SceneObjectTablePanel::Instance() &&
      SceneObjectTablePanel::Instance()->IsActivePage();
  if (sceneObjectsActive &&
      m_controller.GetSceneObjectLabelAt(pickPos.x, pickPos.y, w, h, label, pos,
                                         &uuid)) {
    const bool edited =
        scene_object_primitives::EditPrimitiveObjectByUuid(this, cfg, uuid);
    if (!edited)
      return;

    if (SceneObjectTablePanel::Instance()) {
      SceneObjectTablePanel::Instance()->ReloadData();
      SceneObjectTablePanel::Instance()->SelectByUuid({uuid}, false);
    }

    UpdateScene(false);
    if (Viewer3DPanel::Instance()) {
      Viewer3DPanel::Instance()->UpdateScene();
      Viewer3DPanel::Instance()->Refresh();
    }
    RequestRepaint();
    return;
  }

  if (!m_controller.GetFixtureLabelAt(pickPos.x, pickPos.y, w, h, label, pos,
                                      &uuid))
    return;

  auto &scene = cfg.GetScene();
  auto it = scene.fixtures.find(uuid);
  if (it == scene.fixtures.end())
    return;

  FixturePatchDialog dlg(this, it->second);
  if (dlg.ShowModal() != wxID_OK)
    return;

  it->second.fixtureId = dlg.GetFixtureId();
  int uni = dlg.GetUniverse();
  int ch = dlg.GetChannel();
  if (uni > 0 && ch > 0)
    it->second.address = wxString::Format("%d.%d", uni, ch).ToStdString();
  else
    it->second.address.clear();

  if (FixtureTablePanel::Instance())
    FixtureTablePanel::Instance()->ReloadData();

  UpdateScene(false);
  RequestRepaint();
}

// Completes mouse-driven interaction and applies click or rectangle selections.
void Viewer2DPanel::OnMouseUp(wxMouseEvent &event) {
  if (m_continuousPlacementActive && event.LeftUp()) {
    if (HasCapture())
      ReleaseMouse();
    const bool navigated = m_draggedSincePress;
    m_dragMode = DragMode::Selection;
    m_draggedSincePress = false;
    if (navigated) {
      m_continuousPlacementNeedsPointerAlignment = true;
      AlignContinuousElementToPointer(event.GetPosition());
    } else {
      ConfirmContinuousPlacement();
    }
    RequestRepaint();
    return;
  }

  if (event.LeftUp() && m_dragMode == DragMode::RectSelection) {
    const wxRect dirtyRect =
        BuildSelectionRectDirtyRegion(m_rectSelectStart, m_rectSelectEnd);
    if (HasCapture())
      ReleaseMouse();
    if (m_rectSelecting)
      ApplyRectangleSelection(m_rectSelectStart, m_rectSelectEnd,
                              m_rectSelectionAcrossAllTables, true);
    m_rectSelecting = false;
    m_rectSelectionAcrossAllTables = false;
    m_dragMode = DragMode::None;
    m_dragAxis = DragAxis::None;
    m_dragTarget = DragTarget::None;
    m_dragSelectionUuids.clear();
    m_dragFixtureUuids.clear();
    m_dragTrussUuids.clear();
    m_dragSupportUuids.clear();
    m_dragSceneObjectUuids.clear();
    m_dragSelectionMoved = false;
    m_draggedSincePress = false;
    RequestRepaint(dirtyRect);
    return;
  }

  if (event.LeftUp() && m_dragMode != DragMode::None) {
    if (HasCapture())
      ReleaseMouse();
    if (m_dragMode == DragMode::Selection && m_dragSelectionMoved) {
      CommitActiveMagnetSnap();
      FinalizeSelectionDrag();
    }
    m_pendingMagnetSnap.reset();
    m_dragMode = DragMode::None;
    m_dragAxis = DragAxis::None;
    m_dragTarget = DragTarget::None;
    m_dragSelectionUuids.clear();
    m_dragFixtureUuids.clear();
    m_dragTrussUuids.clear();
    m_dragSupportUuids.clear();
    m_dragSceneObjectUuids.clear();
    m_dragSelectionMoved = false;
    ClearCursorWorldPosition();
  }

  if (!m_enableSelection) {
    m_draggedSincePress = false;
    return;
  }

  if (event.LeftUp() && !m_draggedSincePress) {
    m_lastMousePos = event.GetPosition();
    const bool oldHasHover = m_hasHover;
    const std::string oldHoverUuid = m_hoverUuid;
    const RenderSize renderSize = ResolveRenderSize(this);
    const int w = renderSize.width;
    const int h = renderSize.height;
    if (!IsShownOnScreen()) {
      return;
    }
    if (!renderSize.IsValid())
      return;
    if (!TryBindGlContextForInteraction())
      return;
    std::string uuid;
    const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
    const size_t hiddenLayersHash = BuildHiddenLayersHash();
    bool found = false;
    if (IsCrossTableViewportActionsEnabled())
      found =
          TryResolvePickUuidWithCache(pickPos, w, h, hiddenLayersHash, uuid);
    else if (FixtureTablePanel::Instance() &&
             FixtureTablePanel::Instance()->IsActivePage())
      found = TryResolvePickLabelWithCache(PickQueryKind::FixtureLabel, pickPos,
                                           w, h, hiddenLayersHash, true, uuid);
    else if (TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage())
      found = TryResolvePickLabelWithCache(PickQueryKind::TrussLabel, pickPos,
                                           w, h, hiddenLayersHash, true, uuid);
    else if (HoistTablePanel::Instance() &&
             HoistTablePanel::Instance()->IsActivePage())
      found = TryResolvePickLabelWithCache(PickQueryKind::HoistLabel, pickPos,
                                           w, h, hiddenLayersHash, true, uuid);
    else if (SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage())
      found =
          TryResolvePickLabelWithCache(PickQueryKind::SceneObjectLabel, pickPos,
                                           w, h, hiddenLayersHash, true, uuid);

    ConfigManager &cfg = ConfigManager::Get();
    if (m_measureToolState.enabled) {
      if (!found) {
        ResetViewer2DMeasure(m_measureToolState);
      } else {
        const auto center = ResolveSceneElementCenterByUuid(cfg, uuid);
        if (center) {
          if (!m_measureToolState.hasAnchor ||
              m_measureToolState.hasCommittedTarget) {
            ResetViewer2DMeasure(m_measureToolState);
            m_measureToolState.hasAnchor = true;
            m_measureToolState.anchorUuid = uuid;
            m_measureToolState.anchorWorld = *center;
            m_measureToolState.anchorMeasureWorld = *center;
            const RenderSize anchorRenderSize = ResolveRenderSize(this);
            if (anchorRenderSize.IsValid()) {
              const auto anchorScreen = Viewer2DMeasureWorldToScreen(
                  m_measureToolState.anchorWorld, m_view,
                  anchorRenderSize.width, anchorRenderSize.height, m_zoom,
                  m_offsetX, m_offsetY);
              if (anchorScreen)
                m_lastMousePos =
                    ToLogicalPointFromFramebuffer(this, *anchorScreen);
              else
                m_lastMousePos = event.GetPosition();
            } else {
              m_lastMousePos = event.GetPosition();
            }
          } else {
            m_measureToolState.hasCommittedTarget = true;
            m_measureToolState.committedTargetWorld = *center;
            m_measureToolState.committedTargetMeasureWorld = *center;
            if (m_measureToolState.mode == Viewer2DMeasureMode::EdgeToEdge) {
              const auto anchorBounds = ResolveSceneElementBoundsByUuid(
                  m_controller, cfg, m_measureToolState.anchorUuid);
              const auto targetBounds =
                  ResolveSceneElementBoundsByUuid(m_controller, cfg, uuid);
              if (anchorBounds && targetBounds) {
                const auto nearest = ComputeNearestProjectedBoundsPoints(
                    *anchorBounds, *targetBounds, m_view);
                m_measureToolState.anchorMeasureWorld = nearest.first;
                m_measureToolState.committedTargetMeasureWorld = nearest.second;
              }
            }
            float du = 0.0f;
            float dv = 0.0f;
            switch (m_view) {
            case Viewer2DView::Top:
            case Viewer2DView::Bottom:
              du = m_measureToolState.committedTargetMeasureWorld[0] -
                   m_measureToolState.anchorMeasureWorld[0];
              dv = m_measureToolState.committedTargetMeasureWorld[1] -
                   m_measureToolState.anchorMeasureWorld[1];
              break;
            case Viewer2DView::Front:
              du = m_measureToolState.committedTargetMeasureWorld[0] -
                   m_measureToolState.anchorMeasureWorld[0];
              dv = m_measureToolState.committedTargetMeasureWorld[2] -
                   m_measureToolState.anchorMeasureWorld[2];
              break;
            case Viewer2DView::Side:
              du = m_measureToolState.committedTargetMeasureWorld[1] -
                   m_measureToolState.anchorMeasureWorld[1];
              dv = m_measureToolState.committedTargetMeasureWorld[2] -
                   m_measureToolState.anchorMeasureWorld[2];
              break;
            }
            const float distanceMeters = std::sqrt(du * du + dv * dv);
            const auto distanceUnitSystem = Units::ParseDistanceUnitSystem(
                ConfigManager::Get().GetValue("ui_distance_unit_system"));
            const std::string distanceText =
                Units::FormatDistanceFromMillimeters(
                    static_cast<double>(distanceMeters) * 1000.0,
                    distanceUnitSystem, Units::ValueFormatContext::Label) +
                " " + Units::DistanceUnitSuffix(distanceUnitSystem);
            if (MainWindow::Instance() &&
                MainWindow::Instance()->GetStatusBar()) {
              MainWindow::Instance()->SetStatusText(
                  wxString::FromUTF8((m_measureToolState.mode ==
                                              Viewer2DMeasureMode::EdgeToEdge
                                       ? "Gap measure: "
                                       : "Measure: ") +
                                     distanceText),
                  0);
            }
          }
        }
      }
    }
    bool selectionChanged = false;
    if (found) {
      m_hasHover = true;
      m_hoverUuid = uuid;
      m_controller.SetHighlightUuid(m_hoverUuid);
      bool additive = event.ShiftDown() || event.ControlDown();
      const bool addOnly = event.ControlDown();
      std::vector<std::string> selection;
      if (IsCrossTableViewportActionsEnabled()) {
        auto updateSelection = [&](std::vector<std::string> current) {
          if (additive) {
            auto it = std::find(current.begin(), current.end(), uuid);
            if (it != current.end() && !addOnly)
              current.erase(it);
            else if (it == current.end())
              current.push_back(uuid);
            return current;
          }
          return std::vector<std::string>{uuid};
        };
        const auto &scene = cfg.GetScene();
        if (scene.fixtures.find(uuid) != scene.fixtures.end()) {
          selection = updateSelection(additive ? cfg.GetSelectedFixtures()
                                               : std::vector<std::string>{});
          if (selection != cfg.GetSelectedFixtures()) {
            cfg.PushUndoState("fixture selection");
            cfg.SetSelectedFixtures(selection);
            selectionChanged = true;
          }
          if (FixtureTablePanel::Instance())
            FixtureTablePanel::Instance()->SelectByUuid(selection, false);
        } else if (scene.trusses.find(uuid) != scene.trusses.end()) {
          selection = updateSelection(additive ? cfg.GetSelectedTrusses()
                                               : std::vector<std::string>{});
          if (selection != cfg.GetSelectedTrusses()) {
            cfg.PushUndoState("truss selection");
            cfg.SetSelectedTrusses(selection);
            selectionChanged = true;
          }
          if (TrussTablePanel::Instance())
            TrussTablePanel::Instance()->SelectByUuid(selection, false);
        } else if (scene.supports.find(uuid) != scene.supports.end()) {
          selection = updateSelection(additive ? cfg.GetSelectedSupports()
                                               : std::vector<std::string>{});
          if (selection != cfg.GetSelectedSupports()) {
            cfg.PushUndoState("support selection");
            cfg.SetSelectedSupports(selection);
            selectionChanged = true;
          }
          if (HoistTablePanel::Instance())
            HoistTablePanel::Instance()->SelectByUuid(selection, false);
        } else if (scene.sceneObjects.find(uuid) != scene.sceneObjects.end()) {
          selection = updateSelection(additive ? cfg.GetSelectedSceneObjects()
                                               : std::vector<std::string>{});
          if (selection != cfg.GetSelectedSceneObjects()) {
            cfg.PushUndoState("scene object selection");
            cfg.SetSelectedSceneObjects(selection);
            selectionChanged = true;
          }
          if (SceneObjectTablePanel::Instance())
            SceneObjectTablePanel::Instance()->SelectByUuid(selection, false);
        }
        std::vector<std::string> mergedSelection;
        const auto appendSelection =
            [&](const std::vector<std::string> &source) {
          mergedSelection.insert(mergedSelection.end(), source.begin(),
                                 source.end());
        };
        appendSelection(cfg.GetSelectedFixtures());
        appendSelection(cfg.GetSelectedTrusses());
        appendSelection(cfg.GetSelectedSupports());
        appendSelection(cfg.GetSelectedSceneObjects());
        m_controller.SetSelectedUuids(mergedSelection);
      } else if (FixtureTablePanel::Instance() &&
          FixtureTablePanel::Instance()->IsActivePage()) {
        if (additive)
          selection = cfg.GetSelectedFixtures();
        if (additive) {
          auto it = std::find(selection.begin(), selection.end(), uuid);
          if (it != selection.end() && !addOnly)
            selection.erase(it);
          else if (it == selection.end())
            selection.push_back(uuid);
        } else {
          selection = {uuid};
        }
        if (selection != cfg.GetSelectedFixtures()) {
          cfg.PushUndoState("fixture selection");
          cfg.SetSelectedFixtures(selection);
          selectionChanged = true;
          if (Viewer2DRenderPanel::Instance())
            Viewer2DRenderPanel::Instance()
                ->RefreshLabelControlsFromSelection();
        }
        m_controller.SetSelectedUuids(
            BuildViewerSelectionForTableSelection(cfg, selection, additive),
            additive ? BuildDirectSelection(cfg) : selection);
        FixtureTablePanel::Instance()->SelectByUuid(selection, false);
      } else if (TrussTablePanel::Instance() &&
                 TrussTablePanel::Instance()->IsActivePage()) {
        if (additive)
          selection = cfg.GetSelectedTrusses();
        if (additive) {
          auto it = std::find(selection.begin(), selection.end(), uuid);
          if (it != selection.end() && !addOnly)
            selection.erase(it);
          else if (it == selection.end())
            selection.push_back(uuid);
        } else {
          selection = {uuid};
        }
        if (selection != cfg.GetSelectedTrusses()) {
          cfg.PushUndoState("truss selection");
          cfg.SetSelectedTrusses(selection);
          selectionChanged = true;
        }
        m_controller.SetSelectedUuids(
            BuildViewerSelectionForTableSelection(cfg, selection, additive),
            additive ? BuildDirectSelection(cfg) : selection);
        TrussTablePanel::Instance()->SelectByUuid(selection, false);
      } else if (HoistTablePanel::Instance() &&
                 HoistTablePanel::Instance()->IsActivePage()) {
        if (additive)
          selection = cfg.GetSelectedSupports();
        if (additive) {
          auto it = std::find(selection.begin(), selection.end(), uuid);
          if (it != selection.end() && !addOnly)
            selection.erase(it);
          else if (it == selection.end())
            selection.push_back(uuid);
        } else {
          selection = {uuid};
        }
        if (selection != cfg.GetSelectedSupports()) {
          cfg.PushUndoState("support selection");
          cfg.SetSelectedSupports(selection);
          selectionChanged = true;
        }
        m_controller.SetSelectedUuids(
            BuildViewerSelectionForTableSelection(cfg, selection, additive),
            additive ? BuildDirectSelection(cfg) : selection);
        HoistTablePanel::Instance()->SelectByUuid(selection, false);
      } else if (SceneObjectTablePanel::Instance() &&
                 SceneObjectTablePanel::Instance()->IsActivePage()) {
        if (additive)
          selection = cfg.GetSelectedSceneObjects();
        if (additive) {
          auto it = std::find(selection.begin(), selection.end(), uuid);
          if (it != selection.end() && !addOnly)
            selection.erase(it);
          else if (it == selection.end())
            selection.push_back(uuid);
        } else {
          selection = {uuid};
        }
        if (selection != cfg.GetSelectedSceneObjects()) {
          cfg.PushUndoState("scene object selection");
          cfg.SetSelectedSceneObjects(selection);
          selectionChanged = true;
        }
        m_controller.SetSelectedUuids(
            BuildViewerSelectionForTableSelection(cfg, selection, additive),
            additive ? BuildDirectSelection(cfg) : selection);
        SceneObjectTablePanel::Instance()->SelectByUuid(selection, false);
      }
    } else {
      m_hasHover = false;
      m_hoverUuid.clear();
      m_controller.SetHighlightUuid("");
      const bool hasAnySelection = !cfg.GetSelectedFixtures().empty() ||
                                   !cfg.GetSelectedTrusses().empty() ||
                                   !cfg.GetSelectedSupports().empty() ||
                                   !cfg.GetSelectedSceneObjects().empty();
      if (hasAnySelection) {
        cfg.PushUndoState("clear selection");
        cfg.SetSelectedFixtures({});
        cfg.SetSelectedTrusses({});
        cfg.SetSelectedSupports({});
        cfg.SetSelectedSceneObjects({});
        selectionChanged = true;
        if (Viewer2DRenderPanel::Instance())
          Viewer2DRenderPanel::Instance()->RefreshLabelControlsFromSelection();
      }

      m_controller.SetSelectedUuids({});

      if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->ClearSelection();
      if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->ClearSelection();
      if (HoistTablePanel::Instance())
        HoistTablePanel::Instance()->ClearSelection();
      if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->ClearSelection();
    }
    const bool highlightChanged =
        oldHasHover != m_hasHover || oldHoverUuid != m_hoverUuid;
    if (selectionChanged || highlightChanged)
      RequestRepaint();
  }
  m_draggedSincePress = false;
}

// Opens the active table selection menu when right-clicking empty viewer space.
void Viewer2DPanel::OnRightUp(wxMouseEvent &event) {
  if (m_continuousPlacementActive) {
    CancelContinuousPlacement();
    return;
  }
  if (!m_enableSelection || !event.RightUp()) {
    event.Skip();
    return;
  }

  const RenderSize renderSize = ResolveRenderSize(this);
  const int w = renderSize.width;
  const int h = renderSize.height;
  if (w <= 0 || h <= 0 || !IsShownOnScreen()) {
    event.Skip();
    return;
  }

  if (!TryBindGlContextForInteraction())
    return;

  wxString label;
  wxPoint pos;
  std::string hitUuid;
  const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
  std::string sceneObjectUuid;
  if (m_controller.GetSceneObjectLabelAt(pickPos.x, pickPos.y, w, h, label, pos,
                                         &sceneObjectUuid)) {
    wxMenu menu;
    constexpr int kConvertSceneObjectToTrussId = wxID_HIGHEST + 1300;
    menu.Append(kConvertSceneObjectToTrussId, "Convert to Truss");
    const int selectedId =
        GetPopupMenuSelectionFromUser(menu, event.GetPosition());
    if (selectedId == kConvertSceneObjectToTrussId) {
      ConfigManager::Get().SetSelectedSceneObjects({sceneObjectUuid});
      wxCommandEvent command(wxEVT_MENU, ID_Tools_ConvertSceneObjectsToTruss);
      if (MainWindow::Instance())
        MainWindow::Instance()->ProcessWindowEvent(command);
    }
    return;
  }

  const bool fixturePageActive = FixtureTablePanel::Instance() &&
                                 FixtureTablePanel::Instance()->IsActivePage();
  const bool trussPageActive = TrussTablePanel::Instance() &&
                               TrussTablePanel::Instance()->IsActivePage();
  if (!fixturePageActive && !trussPageActive) {
    event.Skip();
    return;
  }

  const auto &scene = ConfigManager::Get().GetScene();
  std::set<std::string> typeNames;
  std::set<std::string> positionNames;
  bool hasNoPosition = false;
  wxString allLabel;
  wxString typeMenuLabel;

  if (fixturePageActive) {
    if (m_controller.GetFixtureLabelAt(pickPos.x, pickPos.y, w, h, label, pos,
                                       &hitUuid, true)) {
      event.Skip();
      return;
    }
    if (scene.fixtures.empty())
      return;
    for (const auto &[uuid, fixture] : scene.fixtures) {
      if (!fixture.typeName.empty())
        typeNames.insert(fixture.typeName);
      if (fixture.positionName.empty())
        hasNoPosition = true;
      else
        positionNames.insert(fixture.positionName);
    }
    allLabel = "All fixtures";
    typeMenuLabel = "Select by fixture type";
  } else {
    if (m_controller.GetTrussLabelAt(pickPos.x, pickPos.y, w, h, label, pos,
                                     &hitUuid)) {
      event.Skip();
      return;
    }
    if (scene.trusses.empty())
      return;
    for (const auto &[uuid, truss] : scene.trusses) {
      const std::string modelKey = BuildTrussModelSelectionKey(truss);
      if (!modelKey.empty())
        typeNames.insert(modelKey);
      if (truss.positionName.empty())
        hasNoPosition = true;
      else
        positionNames.insert(truss.positionName);
    }
    allLabel = "All trusses";
    typeMenuLabel = "Select by model";
  }

  wxMenu filterMenu;
  auto typeSubmenu = std::make_unique<wxMenu>();
  auto positionSubmenu = std::make_unique<wxMenu>();

  constexpr int kSelectTypeAllId = wxID_HIGHEST + 600;
  constexpr int kSelectTypeBaseId = wxID_HIGHEST + 601;
  constexpr int kSelectPositionNoneId = wxID_HIGHEST + 800;
  constexpr int kSelectPositionBaseId = wxID_HIGHEST + 801;

  typeSubmenu->Append(kSelectTypeAllId, allLabel);

  std::vector<std::string> orderedTypes;
  orderedTypes.reserve(typeNames.size());
  int nextTypeId = kSelectTypeBaseId;
  for (const auto &name : typeNames) {
    orderedTypes.push_back(name);
    typeSubmenu->Append(nextTypeId++, wxString::FromUTF8(name));
  }

  positionSubmenu->Append(kSelectPositionNoneId, "No position");

  std::vector<std::string> orderedPositions;
  orderedPositions.reserve(positionNames.size());
  int nextPositionId = kSelectPositionBaseId;
  for (const auto &name : positionNames) {
    orderedPositions.push_back(name);
    positionSubmenu->Append(nextPositionId++, wxString::FromUTF8(name));
  }

  filterMenu.AppendSubMenu(typeSubmenu.release(), typeMenuLabel);
  filterMenu.AppendSubMenu(positionSubmenu.release(), "Select by position");

  const int selectedId =
      GetPopupMenuSelectionFromUser(filterMenu, event.GetPosition());

  if (selectedId == wxID_NONE)
    return;

  if (selectedId == kSelectTypeAllId) {
    if (fixturePageActive)
      ApplyFixtureSelectionToUi(BuildFixtureSelectionByType(scene, ""),
                                m_controller);
    else
      ApplyTrussSelectionToUi(BuildTrussSelectionByModelKey(scene, ""),
                              m_controller);
    RequestRepaint();
    return;
  }

  if (selectedId >= kSelectTypeBaseId &&
      selectedId < kSelectTypeBaseId + static_cast<int>(orderedTypes.size())) {
    const size_t idx = static_cast<size_t>(selectedId - kSelectTypeBaseId);
    if (fixturePageActive)
      ApplyFixtureSelectionToUi(
          BuildFixtureSelectionByType(scene, orderedTypes[idx]), m_controller);
    else
      ApplyTrussSelectionToUi(
          BuildTrussSelectionByModelKey(scene, orderedTypes[idx]),
                              m_controller);
    RequestRepaint();
    return;
  }

  if (selectedId == kSelectPositionNoneId) {
    if (!hasNoPosition) {
      if (fixturePageActive)
        ApplyFixtureSelectionToUi({}, m_controller);
      else
        ApplyTrussSelectionToUi({}, m_controller);
    } else {
      if (fixturePageActive)
        ApplyFixtureSelectionToUi(
            BuildFixtureSelectionByPosition(scene, "", true), m_controller);
      else
        ApplyTrussSelectionToUi(BuildTrussSelectionByPosition(scene, "", true),
                                m_controller);
    }
    RequestRepaint();
    return;
  }

  if (selectedId >= kSelectPositionBaseId &&
      selectedId <
          kSelectPositionBaseId + static_cast<int>(orderedPositions.size())) {
    const size_t idx = static_cast<size_t>(selectedId - kSelectPositionBaseId);
    if (fixturePageActive)
      ApplyFixtureSelectionToUi(
          BuildFixtureSelectionByPosition(scene, orderedPositions[idx], false),
          m_controller);
    else
      ApplyTrussSelectionToUi(
          BuildTrussSelectionByPosition(scene, orderedPositions[idx], false),
          m_controller);
    RequestRepaint();
    return;
  }
}

void Viewer2DPanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(event)) {
  m_dragMode = DragMode::None;
  m_dragAxis = DragAxis::None;
  m_dragTarget = DragTarget::None;
  m_dragSelectionUuids.clear();
  m_dragFixtureUuids.clear();
  m_dragTrussUuids.clear();
  m_dragSupportUuids.clear();
  m_dragSceneObjectUuids.clear();
  m_dragSelectionMoved = false;
  m_pendingMagnetSnap.reset();
  m_rectSelecting = false;
  m_rectSelectionAcrossAllTables = false;
  ClearCursorWorldPosition();
}

// Aligns the provisional fixture with the raw world position under the pointer.
bool Viewer2DPanel::AlignContinuousElementToPointer(const wxPoint &screenPos) {
  RestorePendingMagnetSnapPreview();
  const auto pointerWorld = ComputeWorldPositionFromScreen(screenPos);
  const auto currentWorld = ComputeSelectionDragCenterMeters();
  if (!pointerWorld || !currentWorld)
    return false;

  ApplySelectionDelta({(*pointerWorld)[0] - (*currentWorld)[0],
                       (*pointerWorld)[1] - (*currentWorld)[1],
                       (*pointerWorld)[2] - (*currentWorld)[2]});
  m_continuousPlacementNeedsPointerAlignment = false;
  m_dragAxis = DragAxis::None;
  m_dragSelectionMoved = true;
  m_lastMousePos = screenPos;
  return true;
}

// Handles pointer-following placement, selection movement, and view panning.
void Viewer2DPanel::OnMouseMove(wxMouseEvent &event) {
  if (m_continuousPlacementActive &&
      !(m_dragMode == DragMode::View && event.Dragging())) {
    const wxPoint pos = event.GetPosition();
    if (m_continuousPlacementNeedsPointerAlignment) {
      AlignContinuousElementToPointer(pos);
    } else {
      int dx = pos.x - m_lastMousePos.x;
      int dy = pos.y - m_lastMousePos.y;
      if (m_axisConstrainedMovementEnabled) {
        if (m_dragAxis == DragAxis::None &&
            (std::abs(dx) >= kSelectionDragStartThresholdPx ||
             std::abs(dy) >= kSelectionDragStartThresholdPx)) {
          m_dragAxis = std::abs(dx) >= std::abs(dy) ? DragAxis::Horizontal
                           : DragAxis::Vertical;
        }
        if (m_dragAxis == DragAxis::Horizontal)
          dy = 0;
        else if (m_dragAxis == DragAxis::Vertical)
          dx = 0;
      } else {
        m_dragAxis = DragAxis::None;
      }
      const float pixelsPerMeter = PIXELS_PER_METER * m_zoom;
      if ((dx != 0 || dy != 0) && pixelsPerMeter > 0.0f) {
        ApplySelectionDelta(
            MapDragDelta(static_cast<float>(dx) / pixelsPerMeter,
                         static_cast<float>(-dy) / pixelsPerMeter));
      }
    }
    m_dragSelectionMoved = true;
    m_lastMousePos = pos;
    RequestRepaint();
    return;
  }
  wxPoint pos = event.GetPosition();
  NotifyCursorWorldPosition(pos);

  if (m_dragMode == DragMode::RectSelection && event.Dragging()) {
    const wxRect oldRect =
        BuildSelectionRectDirtyRegion(m_rectSelectStart, m_rectSelectEnd);
    m_rectSelectEnd = pos;
    m_draggedSincePress = true;
    MarkInteractionActivity();
    const wxRect newRect =
        BuildSelectionRectDirtyRegion(m_rectSelectStart, m_rectSelectEnd);
    RequestRepaint(oldRect.Union(newRect));
    return;
  }

  if (m_dragMode == DragMode::Selection && event.Dragging()) {
    if ((wxGetLocalTimeMillis() - m_dragPressTime).ToLong() <
        kSelectionDragDelayMs) {
      return;
    }

    int dx = pos.x - m_lastMousePos.x;
    int dy = pos.y - m_lastMousePos.y;

    if (!m_dragSelectionMoved &&
        std::abs(dx) < kSelectionDragStartThresholdPx &&
        std::abs(dy) < kSelectionDragStartThresholdPx)
      return;

    if (dx != 0 || dy != 0) {
      if (m_axisConstrainedMovementEnabled) {
        if (m_dragAxis == DragAxis::None) {
          int absDx = std::abs(dx);
          int absDy = std::abs(dy);
          if (absDx >= 3 || absDy >= 3) {
            m_dragAxis =
                absDx >= absDy ? DragAxis::Horizontal : DragAxis::Vertical;
          }
        }

        if (m_dragAxis == DragAxis::Horizontal)
          dy = 0;
        else if (m_dragAxis == DragAxis::Vertical)
          dx = 0;
      }

      if (dx != 0 || dy != 0) {
        float ppm = PIXELS_PER_METER * m_zoom;
        if (ppm > 0.0f) {
          float dxMeters = static_cast<float>(dx) / ppm;
          float dyMeters = static_cast<float>(-dy) / ppm;
          ApplySelectionDelta(MapDragDelta(dxMeters, dyMeters));
          m_draggedSincePress = true;
          MarkInteractionActivity();
          m_dragSelectionMoved = true;
          RequestRepaint();
        }
      }
    }

    m_lastMousePos = pos;
    return;
  }

  if (m_dragMode == DragMode::View && event.Dragging()) {
    int dx = pos.x - m_lastMousePos.x;
    int dy = pos.y - m_lastMousePos.y;
    if (dx == 0 && dy == 0)
      return;
    m_offsetX += dx / m_zoom;
    m_offsetY += dy / m_zoom;
    m_viewMotionSinceLastHoverHitTest = true;
    InvalidatePickCache();
    m_lastMousePos = pos;
    m_draggedSincePress = true;
    MarkInteractionActivity();
    if (m_persistViewState)
      SaveViewToConfig();
    RequestRepaint();
    return;
  }

  if (m_enableSelection) {
    if (pos == m_lastMousePos)
      return;
    m_lastMousePos = pos;
    m_pendingHoverScreenPos = pos;
    RequestRepaint();

    const bool hoistActive = HoistTablePanel::Instance() &&
                             HoistTablePanel::Instance()->IsActivePage();
    if (hoistActive)
      ScheduleHoverLabelRefresh(pos);
  }
}

void Viewer2DPanel::OnMouseWheel(wxMouseEvent &event) {
  MarkInteractionActivity();

  int rotation = event.GetWheelRotation();
  int deltaWheel = event.GetWheelDelta();
  float steps = 0.0f;
  if (deltaWheel != 0)
    steps = static_cast<float>(rotation) / static_cast<float>(deltaWheel);
  float factor = std::pow(1.1f, steps);
  m_zoom *= factor;
  m_viewMotionSinceLastHoverHitTest = true;
  if (m_zoom < 0.1f)
    m_zoom = 0.1f;
  InvalidatePickCache();
  if (m_persistViewState)
    SaveViewToConfig();
  RequestRepaint();
}

// Applies keyboard pan and zoom navigation to the 2D viewport.
bool Viewer2DPanel::TryHandleViewportNavigationKey(int keyCode, bool altDown) {
  const float panStep = 10.0f / m_zoom;

  switch (keyCode) {
  case WXK_LEFT:
    if (altDown)
      m_zoom *= 1.1f;
    else
      m_offsetX += panStep;
    break;
  case WXK_RIGHT:
    if (altDown)
      m_zoom /= 1.1f;
    else
      m_offsetX -= panStep;
    break;
  case WXK_UP:
    if (altDown)
      m_zoom *= 1.1f;
    else
      m_offsetY -= panStep;
    break;
  case WXK_DOWN:
    if (altDown)
      m_zoom /= 1.1f;
    else
      m_offsetY += panStep;
    break;
  default:
    return false;
  }

  if (m_zoom < 0.1f)
    m_zoom = 0.1f;
  InvalidatePickCache();
  if (m_persistViewState)
    SaveViewToConfig();
  RequestRepaint();
  return true;
}

// Handles local keyboard shortcuts when the 2D viewport owns focus.
void Viewer2DPanel::OnKeyDown(wxKeyEvent &event) {
  if (m_continuousPlacementActive && event.GetKeyCode() == WXK_ESCAPE) {
    CancelContinuousPlacement();
    return;
  }
  if (!m_mouseInside) {
    event.Skip();
    return;
  }
  if (gui::IsEditableWidgetFocused(wxWindow::FindFocus())) {
    event.Skip();
    return;
  }

  switch (event.GetKeyCode()) {
  case WXK_ESCAPE:
    if (m_measureToolState.enabled) {
      SetMeasureToolEnabled(false);
      return;
    }
    event.Skip();
    return;
  case 'M':
  case 'm':
    SetMeasureToolEnabled(!m_measureToolState.enabled);
    return;
  case WXK_DELETE:
  case WXK_NUMPAD_DELETE: {
    if (MainWindow::Instance()) {
      wxCommandEvent deleteEvent(wxEVT_MENU, ID_Edit_Delete);
      MainWindow::Instance()->GetEventHandler()->ProcessEvent(deleteEvent);
      return;
    }
    event.Skip();
    return;
  }
  default:
    if (TryHandleViewportNavigationKey(event.GetKeyCode(), event.AltDown()))
      return;
    event.Skip();
    return;
  }
}

void Viewer2DPanel::OnMouseEnter(wxMouseEvent &event) {
  m_mouseInside = true;
  m_fastHoverHasPos = false;
  NotifyCursorWorldPosition(event.GetPosition());
  SetFocus();
  event.Skip();
}

void Viewer2DPanel::OnMouseLeave(wxMouseEvent &event) {
  m_mouseInside = false;
  m_fastHoverHasPos = false;
  ClearCursorWorldPosition();
  if (m_enableSelection) {
    m_hoverHitTestTimer.Stop();
    m_hoverHitTestPending = false;
    ClearHoverState(true);
  }
  event.Skip();
}

void Viewer2DPanel::OnResize(wxSizeEvent &event) {
  if (m_layoutEditAspect && !m_layoutEditViewportSize) {
    m_layoutEditBaseSize.reset();
  }
  InvalidatePickCache();
  RequestRepaint();
  event.Skip();
}
