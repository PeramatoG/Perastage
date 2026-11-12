#pragma once

#include "legend_composition_adapter.h"
#include "layout_pdf_symbol_usage.h"
#include "pdf_draw_commands.h"
#include "viewer2dpdfexporter.h"

namespace layout_pdf_internal {

struct LegendPdfRenderResult {
  LayoutPdfSymbolUsage symbols;
  std::size_t textSubstitutions = 0;
  std::vector<std::string> diagnostics;
};
using LegendCommandStreamRenderer =
    std::function<std::string(const CommandBuffer &, const Mapping &, double)>;

LegendPdfRenderResult RenderLegendPdf(std::ostringstream &out,
    const LayoutLegendExportData &legend, const SymbolDefinitionSnapshot *snapshot,
    double pageHeight, double outputScaleX, double outputScaleY, const PdfFontCatalog &fonts,
    const FloatFormatter &formatter, const viewer2d::LegendSvgResolver &resolveSvg,
    const LegendCommandStreamRenderer &renderCommands,
    layouts::composition::LegendCompositionPlan *composedPlan = nullptr);
} // namespace layout_pdf_internal
