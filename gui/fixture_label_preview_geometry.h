#pragma once
#include "fixture_label_composition.h"
#include <algorithm>
#include <cmath>

namespace label_preview {
struct Point { double x = 0, y = 0; };
struct Transform {
  double centerX = 0, centerY = 0, scale = 1; // Logical pixels per scene metre.
  Point ToScreen(double x, double y) const { return {centerX+x*scale,centerY-y*scale}; }
  bool Hit(const viewer2d::LabelBounds &bounds, Point p) const {
    if (scale <= 0) return false;
    const double x = (p.x-centerX)/scale, y = (centerY-p.y)/scale;
    return x >= bounds.minX && x <= bounds.maxX && y >= bounds.minY && y <= bounds.maxY;
  }
};
enum class DragPhase { Begin, Move, End, Cancel };
class Drag {
public:
  void Begin(Point pointer, const viewer2d::LabelFieldStyle &style, double scale) {
    start_ = pointer; style_ = style; scale_ = scale; active_ = scale > 0; moved_ = false;
  }
  std::optional<viewer2d::LabelFieldStyle> Move(Point pointer) {
    if (!active_) return std::nullopt;
    const double dx = pointer.x-start_.x, dy = pointer.y-start_.y;
    if (!moved_ && std::hypot(dx,dy) < 2) return std::nullopt;
    auto result = style_;
    result.x = std::clamp(style_.x + dx/scale_, -100.0, 100.0);
    result.y = std::clamp(style_.y - dy/scale_, -100.0, 100.0);
    moved_ = result.x != style_.x || result.y != style_.y;
    return result;
  }
  bool Active() const { return active_; }
  bool Changed() const { return moved_; }
  void Finish() { active_ = false; moved_ = false; }
private:
  Point start_;
  viewer2d::LabelFieldStyle style_;
  double scale_ = 1;
  bool active_ = false, moved_ = false;
};
} // namespace label_preview
