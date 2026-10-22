#include "fixture_label_preview.h"
#include "symbols/svg_symbol_presentation.h"
#include <nanovg.h>

void FixtureLabelPreview::DrawSymbol(double scale) {
  const auto ink = darkBackground_ ? nvgRGB(235, 235, 235) : nvgRGB(0, 0, 0);
  nvgStrokeColor(vg_, ink);
  nvgStrokeWidth(vg_, 1);
  const auto contour = [&](const auto &points) {
    if (points.empty()) return;
    bool first = true;
    for (const auto &v : points) {
      const double x = (v.x + symbol_->offsetXmm - symbolAnchor_.x) / 1000;
      const double y = (v.y + symbol_->offsetYmm - symbolAnchor_.y) / 1000;
      const auto p = previewTransform_.ToScreen(symbolTransform_.a * x + symbolTransform_.c * y,
                                               symbolTransform_.b * x + symbolTransform_.d * y);
      if (first) {
        nvgMoveTo(vg_, p.x, p.y);
        first = false;
      } else nvgLineTo(vg_, p.x, p.y);
    }
  };
  if (symbol_) {
    symbols::VisitSvgPresentation(*symbol_, [&](const auto &polygon) {
      nvgBeginPath(vg_);
      contour(polygon.points);
      nvgClosePath(vg_);
      for (const auto &hole : polygon.holes) {
        contour(hole);
        nvgClosePath(vg_);
        nvgPathWinding(vg_, NVG_HOLE);
      }
      nvgFillColor(vg_, nvgRGB(160, 160, 160));
      nvgFill(vg_);
    }, [&](const auto &line) {
      if (line.strokeWidthMm && *line.strokeWidthMm == 0) return;
      nvgStrokeWidth(vg_, line.strokeWidthMm ? *line.strokeWidthMm * scale / 1000 : 1);
      nvgBeginPath(vg_);
      contour(line.points);
      nvgStroke(vg_);
    });
  } else {
    const auto a = previewTransform_.ToScreen(bounds_.minX, bounds_.maxY);
    nvgBeginPath(vg_);
    nvgRect(vg_, a.x, a.y, (bounds_.maxX - bounds_.minX) * scale, (bounds_.maxY - bounds_.minY) * scale);
    nvgStroke(vg_);
  }
}
