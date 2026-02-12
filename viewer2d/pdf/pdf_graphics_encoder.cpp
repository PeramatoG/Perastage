#include "pdf_graphics_encoder.h"

#include <algorithm>
#include <cmath>
#include <iomanip>

FloatFormatter::FloatFormatter(int precision)
    : precision_(std::clamp(precision, 0, 6)) {}

std::string FloatFormatter::Format(double value) const {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(precision_) << value;
  return ss.str();
}

bool GraphicsStateCache::SameColor(const CanvasColor &a, const CanvasColor &b) {
  return std::abs(a.r - b.r) < 1e-6 && std::abs(a.g - b.g) < 1e-6 &&
         std::abs(a.b - b.b) < 1e-6;
}

void GraphicsStateCache::SetStroke(std::ostringstream &out,
                                   const CanvasStroke &stroke,
                                   const FloatFormatter &fmt) {
  if (!joinStyleSet_) { out << "1 j\n"; joinStyleSet_ = true; }
  if (!capStyleSet_) { out << "1 J\n"; capStyleSet_ = true; }
  if (!hasStrokeColor_ || !SameColor(stroke.color, strokeColor_)) {
    out << fmt.Format(stroke.color.r) << ' ' << fmt.Format(stroke.color.g)
        << ' ' << fmt.Format(stroke.color.b) << " RG\n";
    strokeColor_ = stroke.color;
    hasStrokeColor_ = true;
  }
  if (!hasLineWidth_ || std::abs(stroke.width - lineWidth_) > 1e-6) {
    out << fmt.Format(stroke.width) << " w\n";
    lineWidth_ = stroke.width;
    hasLineWidth_ = true;
  }
}

void GraphicsStateCache::SetFill(std::ostringstream &out, const CanvasFill &fill,
                                 const FloatFormatter &fmt) {
  if (!hasFillColor_ || !SameColor(fill.color, fillColor_)) {
    out << fmt.Format(fill.color.r) << ' ' << fmt.Format(fill.color.g) << ' '
        << fmt.Format(fill.color.b) << " rg\n";
    fillColor_ = fill.color;
    hasFillColor_ = true;
  }
}

void AppendLine(std::ostringstream &out, GraphicsStateCache &cache,
                const FloatFormatter &fmt, const Point &a, const Point &b,
                const CanvasStroke &stroke) {
  cache.SetStroke(out, stroke, fmt);
  out << fmt.Format(a.x) << ' ' << fmt.Format(a.y) << " m\n"
      << fmt.Format(b.x) << ' ' << fmt.Format(b.y) << " l\nS\n";
}

void AppendPolygon(std::ostringstream &out, GraphicsStateCache &cache,
                   const FloatFormatter &fmt, const std::vector<Point> &pts,
                   const CanvasStroke &stroke, const CanvasFill *fill) {
  if (pts.size() < 3)
    return;
  auto emit = [&]() {
    out << fmt.Format(pts[0].x) << ' ' << fmt.Format(pts[0].y) << " m\n";
    for (size_t i = 1; i < pts.size(); ++i)
      out << fmt.Format(pts[i].x) << ' ' << fmt.Format(pts[i].y) << " l\n";
    out << "h\n";
  };
  if (stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    emit();
    out << "S\n";
  }
  if (fill) {
    cache.SetFill(out, *fill, fmt);
    emit();
    out << "f\n";
  }
}

void AppendText(std::ostringstream &out, const FloatFormatter &fmt,
                const Point &position, const std::string &text,
                const CanvasTextStyle &style, double scale,
                const PdfFontCatalog *fonts) {
  const PdfFontDefinition *font = fonts ? fonts->Resolve(style.fontFamily) : nullptr;
  const std::string encoded = EncodeWinAnsi(text);
  const double fontSize = std::max(1.0f, style.fontSize) * scale;
  out << "BT\n/" << ((font && font->family.find("Bold") != std::string::npos) ? "F2" : "F1")
      << ' ' << fmt.Format(fontSize) << " Tf\n"
      << fmt.Format(position.x) << ' ' << fmt.Format(position.y)
      << " Td\n(" << encoded << ") Tj\nET\n";
}
