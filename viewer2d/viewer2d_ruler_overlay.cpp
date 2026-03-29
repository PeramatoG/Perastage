#include "viewer2d_ruler_overlay.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#endif

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <cmath>
#include <sstream>

namespace viewer2d {
namespace {
constexpr float kPixelsPerMeter = 25.0f;
constexpr float kMetricShortTickStepMeters = 0.5f;
constexpr float kMetricLongTickStepMeters = 1.0f;
constexpr float kMetersPerInch = 0.0254f;
constexpr float kInchesPerFoot = 12.0f;
constexpr float kImperialShortTickStepMeters = 6.0f * kMetersPerInch;
constexpr float kImperialLongTickStepMeters = kInchesPerFoot * kMetersPerInch;
constexpr float kRulerLabelFontSize = 3.0f;
constexpr float kRulerLabelOffsetMeters = 0.12f;
constexpr float kRulerZeroOriginMeters = 0.0f;

enum class RulerTickKind { None, Short, Long };

bool IsNearMultiple(float value, float step) {
  if (step <= 0.0f)
    return false;
  const float ratio = value / step;
  const float rounded = std::round(ratio);
  return std::fabs(ratio - rounded) < 0.0005f;
}

float ResolveTickStepMeters(bool useImperialUnits) {
  return useImperialUnits ? kImperialShortTickStepMeters
                          : kMetricShortTickStepMeters;
}

RulerTickKind ResolveTickKind(float valueMeters, float rulerOriginMeters,
                              bool useImperialUnits) {
  const float relativeMeters = valueMeters - rulerOriginMeters;
  if (useImperialUnits) {
    if (IsNearMultiple(relativeMeters, kImperialLongTickStepMeters))
      return RulerTickKind::Long;
    if (IsNearMultiple(relativeMeters, kImperialShortTickStepMeters))
      return RulerTickKind::Short;
    return RulerTickKind::None;
  }

  if (IsNearMultiple(relativeMeters, kMetricLongTickStepMeters))
    return RulerTickKind::Long;
  if (IsNearMultiple(relativeMeters, kMetricShortTickStepMeters))
    return RulerTickKind::Short;
  return RulerTickKind::None;
}

float ResolveTickLengthMeters(RulerTickKind kind, float shortTickMeters,
                              float longTickMeters) {
  switch (kind) {
  case RulerTickKind::Long:
    return longTickMeters;
  case RulerTickKind::Short:
    return shortTickMeters;
  case RulerTickKind::None:
    break;
  }
  return 0.0f;
}

float ComputeStartTick(float minValue, float rulerOriginMeters,
                       float tickStepMeters) {
  const float startIndex =
      std::ceil((minValue - rulerOriginMeters) / tickStepMeters);
  return rulerOriginMeters + startIndex * tickStepMeters;
}

float VerticalTickDirection(Viewer2DView view) {
  // In Side view the vertical ruler represents Z, and its ticks must point to
  // the opposite side to match the requested orientation.
  return view == Viewer2DView::Side ? -1.0f : 1.0f;
}

struct WorldPoint {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

WorldPoint MapViewCoordinatesToWorld(float u, float v, Viewer2DView view) {
  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    return {u, v, 0.0f};
  case Viewer2DView::Front:
    return {u, 0.0f, v};
  case Viewer2DView::Side:
    return {0.0f, u, v};
  }
  return {u, v, 0.0f};
}

void EmitViewLine(float u0, float v0, float u1, float v1, Viewer2DView view) {
  const WorldPoint p0 = MapViewCoordinatesToWorld(u0, v0, view);
  const WorldPoint p1 = MapViewCoordinatesToWorld(u1, v1, view);
  glVertex3f(p0.x, p0.y, p0.z);
  glVertex3f(p1.x, p1.y, p1.z);
}

void DrawHorizontalRuler(float minU, float maxU, float vAxis,
                         float rulerOriginMeters, float shortTickMeters,
                         float longTickMeters,
                         bool useImperialUnits, Viewer2DView view) {
  glBegin(GL_LINES);
  EmitViewLine(minU, vAxis, maxU, vAxis, view);

  const float tickStepMeters = ResolveTickStepMeters(useImperialUnits);
  const float startTick =
      ComputeStartTick(minU, rulerOriginMeters, tickStepMeters);
  for (float u = startTick; u <= maxU + 0.0001f; u += tickStepMeters) {
    const auto tickKind =
        ResolveTickKind(u, rulerOriginMeters, useImperialUnits);
    const float tick =
        ResolveTickLengthMeters(tickKind, shortTickMeters, longTickMeters);
    if (tick <= 0.0f)
      continue;
    EmitViewLine(u, vAxis, u, vAxis + tick, view);
  }
  glEnd();
}

void DrawVerticalRuler(float minV, float maxV, float uAxis,
                       float rulerOriginMeters, float shortTickMeters,
                       float longTickMeters,
                       bool useImperialUnits, Viewer2DView view) {
  glBegin(GL_LINES);
  EmitViewLine(uAxis, minV, uAxis, maxV, view);

  const float tickStepMeters = ResolveTickStepMeters(useImperialUnits);
  const float startTick =
      ComputeStartTick(minV, rulerOriginMeters, tickStepMeters);
  const float tickDirection = VerticalTickDirection(view);
  for (float v = startTick; v <= maxV + 0.0001f; v += tickStepMeters) {
    const auto tickKind =
        ResolveTickKind(v, rulerOriginMeters, useImperialUnits);
    const float tick =
        ResolveTickLengthMeters(tickKind, shortTickMeters, longTickMeters);
    if (tick <= 0.0f)
      continue;
    EmitViewLine(uAxis, v, uAxis + tick * tickDirection, v, view);
  }
  glEnd();
}

CanvasStroke BuildRulerStroke(bool darkMode) {
  CanvasStroke stroke;
  stroke.color = darkMode ? CanvasColor{1.0f, 1.0f, 1.0f, 1.0f}
                          : CanvasColor{0.0f, 0.0f, 0.0f, 1.0f};
  stroke.width = 1.0f;
  return stroke;
}

struct ActiveRulers {
  float horizontalAxisMeters = 0.0f;
  float verticalAxisMeters = 0.0f;
};

ActiveRulers ResolveActiveRulers(const RulerOverlayViewState &state) {
  switch (state.view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    return {state.xRulerPositionMeters, state.yRulerPositionMeters};
  case Viewer2DView::Front:
    return {state.xRulerPositionMeters, state.zRulerPositionMeters};
  case Viewer2DView::Side:
    return {state.yRulerPositionMeters, state.zRulerPositionMeters};
  }
  return {state.xRulerPositionMeters, state.yRulerPositionMeters};
}

std::string FormatRulerOffsetLabel(float valueMeters) {
  if (std::fabs(valueMeters) < 0.0001f)
    return "0 m";

  std::ostringstream out;
  if (std::fabs(valueMeters - std::round(valueMeters)) < 0.0001f) {
    out << static_cast<int>(std::lround(valueMeters));
  } else {
    out.setf(std::ios::fixed);
    out.precision(1);
    out << valueMeters;
  }
  out << " m";
  return out.str();
}

std::string FormatImperialRulerOffsetLabel(float valueMeters) {
  if (std::fabs(valueMeters) < 0.0001f)
    return "0 ft";

  const double feet = static_cast<double>(valueMeters) /
                      static_cast<double>(kImperialLongTickStepMeters);
  std::ostringstream out;
  out << static_cast<int>(std::lround(feet)) << " ft";
  return out.str();
}

CanvasTextStyle BuildRulerLabelStyle(const CanvasStroke &stroke) {
  CanvasTextStyle style;
  style.fontSize = kRulerLabelFontSize;
  style.color = stroke.color;
  return style;
}

float WorldToScreenX(float worldX, const RulerOverlayViewState &state,
                     float pixelsPerMeter) {
  (void)pixelsPerMeter;
  const float offsetMetersX = state.offsetPixelsX / kPixelsPerMeter;
  return static_cast<float>(state.width) * 0.5f +
         (worldX + offsetMetersX) * kPixelsPerMeter * state.zoom;
}

float WorldToScreenY(float worldY, const RulerOverlayViewState &state,
                     float pixelsPerMeter) {
  (void)pixelsPerMeter;
  const float offsetMetersY = state.offsetPixelsY / kPixelsPerMeter;
  return static_cast<float>(state.height) * 0.5f -
         (worldY + offsetMetersY) * kPixelsPerMeter * state.zoom;
}
} // namespace

void DrawRulerOverlay(const RulerOverlayViewState &state, bool darkMode) {
  if (state.width <= 0 || state.height <= 0 || state.zoom <= 0.0f)
    return;

  const float pixelsPerMeter = kPixelsPerMeter * state.zoom;
  if (pixelsPerMeter <= 0.0f)
    return;

  const float offsetMetersX = state.offsetPixelsX / kPixelsPerMeter;
  const float offsetMetersY = state.offsetPixelsY / kPixelsPerMeter;
  const float shortTickMeters = std::max(state.smallTickMeters, 0.01f);
  const float longTickMeters =
      std::max(std::max(state.largeTickMeters, shortTickMeters),
               shortTickMeters);
  const ActiveRulers activeRulers = ResolveActiveRulers(state);
  const float yAxis = activeRulers.horizontalAxisMeters;
  const float xAxis = activeRulers.verticalAxisMeters;
  const float halfW = static_cast<float>(state.width) * 0.5f / pixelsPerMeter;
  const float halfH = static_cast<float>(state.height) * 0.5f / pixelsPerMeter;

  const float minX = -halfW - offsetMetersX;
  const float maxX = halfW - offsetMetersX;
  const float minY = -halfH - offsetMetersY;
  const float maxY = halfH - offsetMetersY;

  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);

  if (darkMode)
    glColor3f(1.0f, 1.0f, 1.0f);
  else
    glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(1.0f);

  DrawHorizontalRuler(minX, maxX, yAxis, kRulerZeroOriginMeters, shortTickMeters,
                      longTickMeters, state.useImperialUnits,
                      state.view);
  DrawVerticalRuler(minY, maxY, xAxis, kRulerZeroOriginMeters, shortTickMeters,
                    longTickMeters, state.useImperialUnits,
                    state.view);

  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

void EmitRulerToCanvas(const RulerOverlayViewState &state, bool darkMode,
                       ICanvas2D &canvas) {
  if (state.width <= 0 || state.height <= 0 || state.zoom <= 0.0f)
    return;

  const float pixelsPerMeter = kPixelsPerMeter * state.zoom;
  if (pixelsPerMeter <= 0.0f)
    return;

  const float offsetMetersX = state.offsetPixelsX / kPixelsPerMeter;
  const float offsetMetersY = state.offsetPixelsY / kPixelsPerMeter;
  const float halfW = static_cast<float>(state.width) * 0.5f / pixelsPerMeter;
  const float halfH = static_cast<float>(state.height) * 0.5f / pixelsPerMeter;
  const float minX = -halfW - offsetMetersX;
  const float maxX = halfW - offsetMetersX;
  const float minY = -halfH - offsetMetersY;
  const float maxY = halfH - offsetMetersY;
  const float shortTickMeters = std::max(state.smallTickMeters, 0.01f);
  const float longTickMeters =
      std::max(std::max(state.largeTickMeters, shortTickMeters),
               shortTickMeters);
  auto stroke = BuildRulerStroke(darkMode);
  const ActiveRulers activeRulers = ResolveActiveRulers(state);

  const float xRulerY = activeRulers.horizontalAxisMeters;
  const float yRulerX = activeRulers.verticalAxisMeters;
  canvas.DrawLine(minX, xRulerY, maxX, xRulerY, stroke);
  canvas.DrawLine(yRulerX, minY, yRulerX, maxY, stroke);

  const float tickStepMeters = ResolveTickStepMeters(state.useImperialUnits);
  const float startX =
      ComputeStartTick(minX, kRulerZeroOriginMeters, tickStepMeters);
  const auto textStyle = BuildRulerLabelStyle(stroke);
  for (float x = startX; x <= maxX + 0.0001f; x += tickStepMeters) {
    const auto tickKind =
        ResolveTickKind(x, kRulerZeroOriginMeters, state.useImperialUnits);
    const float tick =
        ResolveTickLengthMeters(tickKind, shortTickMeters, longTickMeters);
    if (tick <= 0.0f)
      continue;
    canvas.DrawLine(x, xRulerY, x, xRulerY + tick, stroke);
    if (tickKind == RulerTickKind::Long) {
      const float labelOffsetMeters = x - kRulerZeroOriginMeters;
      const std::string label =
          state.useImperialUnits ? FormatImperialRulerOffsetLabel(labelOffsetMeters)
                                 : FormatRulerOffsetLabel(labelOffsetMeters);
      canvas.DrawText(x, xRulerY + tick + kRulerLabelOffsetMeters, label,
                      textStyle);
    }
  }

  const float startY =
      ComputeStartTick(minY, kRulerZeroOriginMeters, tickStepMeters);
  const float verticalTickDirection = VerticalTickDirection(state.view);
  for (float y = startY; y <= maxY + 0.0001f; y += tickStepMeters) {
    const auto tickKind =
        ResolveTickKind(y, kRulerZeroOriginMeters, state.useImperialUnits);
    const float tick =
        ResolveTickLengthMeters(tickKind, shortTickMeters, longTickMeters);
    if (tick <= 0.0f)
      continue;
    canvas.DrawLine(yRulerX, y, yRulerX + tick * verticalTickDirection, y,
                    stroke);
    if (tickKind == RulerTickKind::Long) {
      const float labelOffsetMeters = y - kRulerZeroOriginMeters;
      const std::string label =
          state.useImperialUnits ? FormatImperialRulerOffsetLabel(labelOffsetMeters)
                                 : FormatRulerOffsetLabel(labelOffsetMeters);
      canvas.DrawText(yRulerX + tick + kRulerLabelOffsetMeters, y, label,
                      textStyle);
    }
  }
}

std::vector<RulerScreenLabel>
BuildRulerScreenLabels(const RulerOverlayViewState &state) {
  std::vector<RulerScreenLabel> labels;
  if (state.width <= 0 || state.height <= 0 || state.zoom <= 0.0f)
    return labels;

  const float pixelsPerMeter = kPixelsPerMeter * state.zoom;
  if (pixelsPerMeter <= 0.0f)
    return labels;

  const float offsetMetersX = state.offsetPixelsX / kPixelsPerMeter;
  const float offsetMetersY = state.offsetPixelsY / kPixelsPerMeter;
  const float halfW = static_cast<float>(state.width) * 0.5f / pixelsPerMeter;
  const float halfH = static_cast<float>(state.height) * 0.5f / pixelsPerMeter;
  const float minX = -halfW - offsetMetersX;
  const float maxX = halfW - offsetMetersX;
  const float minY = -halfH - offsetMetersY;
  const float maxY = halfH - offsetMetersY;
  const float shortTickMeters = std::max(state.smallTickMeters, 0.01f);
  const float longTickMeters =
      std::max(std::max(state.largeTickMeters, shortTickMeters),
               shortTickMeters);
  const ActiveRulers activeRulers = ResolveActiveRulers(state);
  const float xRulerY = activeRulers.horizontalAxisMeters;
  const float yRulerX = activeRulers.verticalAxisMeters;

  const float tickStepMeters = ResolveTickStepMeters(state.useImperialUnits);
  const float startX =
      ComputeStartTick(minX, kRulerZeroOriginMeters, tickStepMeters);
  for (float x = startX; x <= maxX + 0.0001f; x += tickStepMeters) {
    const auto tickKind =
        ResolveTickKind(x, kRulerZeroOriginMeters, state.useImperialUnits);
    if (tickKind != RulerTickKind::Long)
      continue;
    const float tick =
        ResolveTickLengthMeters(tickKind, shortTickMeters, longTickMeters);
    const float worldY = xRulerY + tick + kRulerLabelOffsetMeters;
    const float labelOffsetMeters = x - kRulerZeroOriginMeters;
    const std::string label =
        state.useImperialUnits ? FormatImperialRulerOffsetLabel(labelOffsetMeters)
                               : FormatRulerOffsetLabel(labelOffsetMeters);
    labels.push_back({WorldToScreenX(x, state, pixelsPerMeter),
                      WorldToScreenY(worldY, state, pixelsPerMeter),
                      label, true, false});
  }

  const float startY =
      ComputeStartTick(minY, kRulerZeroOriginMeters, tickStepMeters);
  for (float y = startY; y <= maxY + 0.0001f; y += tickStepMeters) {
    const auto tickKind =
        ResolveTickKind(y, kRulerZeroOriginMeters, state.useImperialUnits);
    if (tickKind != RulerTickKind::Long)
      continue;
    const float tick =
        ResolveTickLengthMeters(tickKind, shortTickMeters, longTickMeters);
    const float worldX = yRulerX + tick + kRulerLabelOffsetMeters;
    const float labelOffsetMeters = y - kRulerZeroOriginMeters;
    const std::string label =
        state.useImperialUnits ? FormatImperialRulerOffsetLabel(labelOffsetMeters)
                               : FormatRulerOffsetLabel(labelOffsetMeters);
    labels.push_back({WorldToScreenX(worldX, state, pixelsPerMeter),
                      WorldToScreenY(y, state, pixelsPerMeter),
                      label, false, true});
  }
  return labels;
}

} // namespace viewer2d
