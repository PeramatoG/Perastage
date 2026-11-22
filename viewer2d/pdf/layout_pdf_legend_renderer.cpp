#include "layout_pdf_legend_renderer.h"
#include "pdf_unicode_fonts.h"
#include "paper_stroke_mapping.h"
#include "layout_pdf_page_mapping.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace layout_pdf_internal {
namespace {
using namespace layouts::composition;
std::array<double, 3> FillRgb(const std::optional<std::string> &hex) {
  constexpr double component = 224.0 / 255.0;
  constexpr std::array<double, 3> gray = {component, component, component};
  if (!hex) return gray;
  std::string value = *hex;
  value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
    return std::isspace(c) != 0;
  }), value.end());
  if (!value.empty() && value.front() == '#') value.erase(value.begin());
  if (value.size() != 6 || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
      })) return gray;
  return {std::stoi(value.substr(0, 2), nullptr, 16) / 255.0,
          std::stoi(value.substr(2, 2), nullptr, 16) / 255.0,
          std::stoi(value.substr(4, 2), nullptr, 16) / 255.0};
}
} // namespace

LegendPdfRenderResult RenderLegendPdf(std::ostringstream &out,
    const LayoutLegendExportData &legend, const SymbolDefinitionSnapshot *snapshot,
    double pageHeight, double outputScaleX, double outputScaleY, PdfUnicodeFonts &ink,
    const FloatFormatter &formatter, const viewer2d::LegendSvgResolver &resolveSvg,
    const LegendCommandStreamRenderer &renderCommands, LegendCompositionPlan *composedPlan) {
  LegendPdfRenderResult result;
  if (legend.frame.width <= 0 || legend.frame.height <= 0) return result;
  auto resources = viewer2d::ResolveLegendComposition(legend.items, snapshot, resolveSvg);
  auto &input = resources.input;
  input.width = legend.frame.width;
  input.height = legend.frame.height;
  input.showChannelColumn = legend.showChannelColumn;
  const typography::TtfFontMetrics empty;
  const auto plan = ComposeLegend(input, {empty, empty, &ink.Catalog(), legend.fontFamily, typography::FontUsage::Pdf});
  if (composedPlan) *composedPlan = plan;
  result.textSubstitutions = plan.unsupportedGlyphs;
  result.diagnostics = plan.diagnostics;
  ScopedDocumentPageMapping pageMapping(out, pageHeight, outputScaleX, outputScaleY, formatter);
  const double frameX = legend.frame.x;
  const double frameTop = pageHeight - legend.frame.y;
  const auto x = [&](double local) { return formatter.Format(frameX + local); };
  const auto y = [&](double local) { return formatter.Format(frameTop - local); };
  auto rect = [&](const Rect &r) {
    out << x(r.x) << ' ' << y(r.y + r.height) << ' '
        << formatter.Format(r.width) << ' ' << formatter.Format(r.height) << " re ";
  };
  out << "q\n";
  rect(plan.clip); out << "W n\n1 1 1 rg ";
  rect(plan.clip); out << "f\n";
  out << "0.784 0.784 0.784 RG 1 w " << x(plan.separatorStart.x) << ' '
      << y(plan.separatorStart.y) << " m " << x(plan.separatorEnd.x) << ' '
      << y(plan.separatorEnd.y) << " l S\n";
  for (const auto &placement : plan.symbols) {
    const auto &resolved = resources.symbols[placement.geometry.reference];
    const auto &bounds = placement.geometry.bounds;
    out << "q\n"; rect(placement.slot); out << "W n\n";
    if (resolved.svg) {
      const auto &svg = *resolved.svg;
      const auto rgb = FillRgb(legend.items[placement.row].symbolFillHex);
      out << formatter.Format(rgb[0]) << ' ' << formatter.Format(rgb[1]) << ' '
          << formatter.Format(rgb[2]) << " rg\n";
      auto point = [&](const PerastageSvgPoint &p) {
        out << x(placement.draw.x + (p.x + svg.offsetXmm - bounds.x) * placement.scale)
            << ' ' << y(placement.draw.y + (p.y + svg.offsetYmm - bounds.y) * placement.scale);
      };
      auto polygonPath = [&](const std::vector<PerastageSvgPoint> &points) {
        if (points.size() < 3) return;
        point(points[0]); out << " m\n";
        for (std::size_t i = 1; i < points.size(); ++i) { point(points[i]); out << " l\n"; }
        out << "h\n";
      };
      for (const auto &polygon : svg.fills) {
        if (polygon.points.size() < 3) continue;
        polygonPath(polygon.points);
        for (const auto &hole : polygon.holes) polygonPath(hole);
        out << (polygon.holes.empty() ? "f\n" : "f*\n");
      }
      out << "0 0 0 RG 1 w\n";
      for (const auto &line : svg.strokes) {
        if (line.points.size() < 2) continue;
        point(line.points[0]); out << " m\n";
        for (std::size_t i = 1; i < line.points.size(); ++i) { point(line.points[i]); out << " l\n"; }
        out << "S\n";
      }
      ++result.symbols.perastageSymbolInstances;
    } else if (resolved.commands) {
      Mapping mapping;
      mapping.minX = bounds.x;
      mapping.minY = bounds.y;
      mapping.scale = placement.scale;
      mapping.offsetX = frameX + placement.commandOriginX + bounds.x * placement.scale;
      mapping.offsetY = frameTop - placement.draw.y - placement.draw.height;
      mapping.drawHeight = placement.draw.height;
      mapping.flipY = false;
      out << renderCommands(resolved.commands->localCommands, mapping, viewer2d::PaperStrokeScale(placement.scale, 1.0, placement.geometry.strokeScale));
      ++result.symbols.fallbackSymbolInstances;
    }
    out << "Q\n";
  }
  for (const auto &line : plan.text) {
    out << "q\n"; rect(line.clip); out << "W n\n";
    out << "0.078431 0.078431 0.078431 rg\n";
    ink.AppendRun(out, formatter, line.glyphRun, frameX + line.baseline.x, frameTop - line.baseline.y);
    out << "Q\n";
  }
  out << "Q\nq\n0 0 0 RG " << formatter.Format(plan.borderWidth) << " w ";
  rect(plan.clip); out << "S\nQ\n";
  return result;
}
} // namespace layout_pdf_internal
