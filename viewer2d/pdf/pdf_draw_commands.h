#pragma once

#include "pdf_font_metrics.h"
#include "pdf_objects.h"
#include "viewer2dcommandrenderer.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace layout_pdf_internal {

struct Point { double x = 0.0; double y = 0.0; };
struct Transform { double scale = 1.0; double offsetX = 0.0; double offsetY = 0.0; };
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

class GraphicsStateCache {
public:
  void SetStroke(std::ostringstream &out, const CanvasStroke &stroke, const FloatFormatter &fmt);
  void SetFill(std::ostringstream &out, const CanvasFill &fill, const FloatFormatter &fmt);

private:
  CanvasColor strokeColor_{};
  CanvasColor fillColor_{};
  double lineWidth_ = -1.0;
  bool hasStrokeColor_ = false;
  bool hasFillColor_ = false;
  bool hasLineWidth_ = false;
  bool joinStyleSet_ = false;
  bool capStyleSet_ = false;
};

Point Apply(const Transform &t, double x, double y);
Point MapWithMapping(double x, double y, const Mapping &mapping);
Point MapPointWithTransform(double x, double y, const Transform &current,
                            const Mapping &mapping);
Transform2D TransformFromCanvas(const CanvasTransform &transform);
SymbolBounds ComputeSymbolBounds(const std::vector<CanvasCommand> &commands);

void AppendLine(std::ostringstream &out, GraphicsStateCache &cache, const FloatFormatter &fmt,
                const Point &a, const Point &b, const CanvasStroke &stroke);
void AppendPolyline(std::ostringstream &out, GraphicsStateCache &cache, const FloatFormatter &fmt,
                    const std::vector<Point> &pts, const CanvasStroke &stroke);
void AppendPolygon(std::ostringstream &out, GraphicsStateCache &cache, const FloatFormatter &fmt,
                   const std::vector<Point> &pts, const CanvasStroke &stroke, const CanvasFill *fill);
void AppendRectangle(std::ostringstream &out, GraphicsStateCache &cache, const FloatFormatter &fmt,
                     const Point &origin, double w, double h, const CanvasStroke &stroke,
                     const CanvasFill *fill);
void AppendCircle(std::ostringstream &out, GraphicsStateCache &cache, const FloatFormatter &fmt,
                  const Point &center, double radius, const CanvasStroke &stroke,
                  const CanvasFill *fill);
void AppendText(std::ostringstream &out, const FloatFormatter &fmt, const Point &pos,
                const TextCommand &cmd, const CanvasTextStyle &style, double scale,
                const PdfFontCatalog *fonts);
void AppendSymbolInstance(std::ostringstream &out, const FloatFormatter &fmt,
                          const Mapping &mapping, const Transform2D &transform,
                          const std::string &name);
void EmitCommandStroke(std::ostringstream &content, GraphicsStateCache &cache,
                       const FloatFormatter &formatter, const Mapping &mapping,
                       const Transform &current, const CanvasCommand &command,
                       const RenderOptions &options);
void EmitCommandFill(std::ostringstream &content, GraphicsStateCache &cache,
                     const FloatFormatter &formatter, const Mapping &mapping,
                     const Transform &current, const CanvasCommand &command);

} // namespace layout_pdf_internal
