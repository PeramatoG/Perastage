/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */

#pragma once

#include <array>
#include <optional>

namespace viewer2d::interaction {

using WorldPoint = std::array<float, 3>;

struct LinePointSelectionResult {
  WorldPoint first;
  WorldPoint second;
};

// Owns GUI-independent two-point line-selection lifecycle state.
class Viewer2DLinePointSelectionSession {
public:
  // Starts a fresh selection constrained to the supplied world-space line.
  void Begin(WorldPoint lineStart, WorldPoint lineEnd);
  // Updates the projected pointer preview.
  void UpdatePreview(WorldPoint point);
  // Accepts a projected point and returns a pair when selection completes.
  std::optional<LinePointSelectionResult> AcceptPoint(WorldPoint point);
  // Arms suppression of the matching mouse-up event.
  void MarkConsumeNextMouseUp();
  // Consumes and clears the pending mouse-up suppression flag.
  bool ConsumeNextMouseUp();
  // Cancels selection and clears all session-owned state.
  void Reset();

  // Reports whether line-point selection is active.
  bool IsActive() const { return m_active; }
  // Reports whether the next mouse-up belongs to an accepted point click.
  bool ShouldConsumeNextMouseUp() const { return m_consumeNextMouseUp; }
  // Returns the line's world-space start point.
  const WorldPoint &LineStart() const { return m_lineStart; }
  // Returns the line's world-space end point.
  const WorldPoint &LineEnd() const { return m_lineEnd; }
  // Returns the first accepted point when present.
  const std::optional<WorldPoint> &FirstPoint() const { return m_firstPoint; }
  // Returns the current projected pointer preview when present.
  const std::optional<WorldPoint> &PreviewPoint() const {
    return m_previewPoint;
  }

private:
  bool m_active = false;
  bool m_consumeNextMouseUp = false;
  WorldPoint m_lineStart{};
  WorldPoint m_lineEnd{};
  std::optional<WorldPoint> m_firstPoint;
  std::optional<WorldPoint> m_previewPoint;
};

} // namespace viewer2d::interaction
