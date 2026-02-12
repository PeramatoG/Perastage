#pragma once

#include "canvas2d.h"
#include "font_metrics.h"

#include <sstream>
#include <unordered_map>

struct Point {
  double x = 0.0;
  double y = 0.0;
};

struct Transform {
  double scale = 1.0;
  double offsetX = 0.0;
  double offsetY = 0.0;
};

struct Mapping {
  double minX = 0.0;
  double minY = 0.0;
  double scale = 1.0;
  double offsetX = 0.0;
  double offsetY = 0.0;
  double drawHeight = 0.0;
  bool flipY = true;
};

struct RenderOptions {
  bool includeText = true;
  const std::unordered_map<std::string, std::string> *symbolKeyNames = nullptr;
  const std::unordered_map<uint32_t, std::string> *symbolIdNames = nullptr;
  const PdfFontCatalog *fonts = nullptr;
  double strokeScale = 1.0;
};

class FloatFormatter {
public:
  explicit FloatFormatter(int precision);
  std::string Format(double value) const;

private:
  int precision_;
};

class GraphicsStateCache {
public:
  void SetStroke(std::ostringstream &out, const CanvasStroke &stroke,
                 const FloatFormatter &fmt);
  void SetFill(std::ostringstream &out, const CanvasFill &fill,
               const FloatFormatter &fmt);

private:
  static bool SameColor(const CanvasColor &a, const CanvasColor &b);
  CanvasColor strokeColor_{};
  CanvasColor fillColor_{};
  double lineWidth_ = -1.0;
  bool hasStrokeColor_ = false;
  bool hasFillColor_ = false;
  bool hasLineWidth_ = false;
  bool joinStyleSet_ = false;
  bool capStyleSet_ = false;
};

void AppendLine(std::ostringstream &out, GraphicsStateCache &cache,
                const FloatFormatter &fmt, const Point &a, const Point &b,
                const CanvasStroke &stroke);

void AppendPolygon(std::ostringstream &out, GraphicsStateCache &cache,
                   const FloatFormatter &fmt, const std::vector<Point> &pts,
                   const CanvasStroke &stroke, const CanvasFill *fill);

void AppendText(std::ostringstream &out, const FloatFormatter &fmt,
                const Point &position, const std::string &text,
                const CanvasTextStyle &style, double scale,
                const PdfFontCatalog *fonts);
