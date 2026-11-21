#include "layout_pdf_text_renderer.h"
#include "layout_pdf_page_mapping.h"
#include "pdf_unicode_fonts.h"

namespace layout_pdf_internal {
void RenderLayoutTextPdf(std::ostringstream &out, const LayoutTextExportData &text,
    double pageH, double scaleX, double scaleY, PdfUnicodeFonts &ink,
    const FloatFormatter &fmt) {
  const double width = text.frame.width, height = text.frame.height;
  if (width <= 0 || height <= 0) return;
  ScopedDocumentPageMapping pageMapping(out, pageH, scaleX, scaleY, fmt);
  const double top = pageH - text.frame.y;
  const auto rect = fmt.Format(text.frame.x) + ' ' + fmt.Format(top - height) + ' ' +
                    fmt.Format(width) + ' ' + fmt.Format(height) + " re ";
  out << "q\n" << rect << "W n\n";
  if (text.solidBackground) out << "1 1 1 rg " << rect << "f\n";

  const auto plan = typography::ComposeTextBlock(text, width, height, ink.Catalog());
  for (const auto &warning : plan.diagnostics) ink.AddDiagnostic(warning);
  const auto color = [&](const auto &rgb) {
    out << fmt.Format(rgb[0] / 255.0) << ' ' << fmt.Format(rgb[1] / 255.0) << ' '
        << fmt.Format(rgb[2] / 255.0) << " rg\n";
  };
  for (const auto &entry : plan.glyphs) {
    color(entry.color);
    ink.AppendGlyph(out, fmt, entry.glyph, text.frame.x + entry.x, top - entry.baseline, entry.size);
  }
  for (const auto &line : plan.decorations) {
    color(line.color);
    out << fmt.Format(text.frame.x + line.x) << ' ' << fmt.Format(top - line.y - line.height)
        << ' ' << fmt.Format(line.width) << ' ' << fmt.Format(line.height) << " re f\n";
  }
  out << "Q\n";
  if (text.drawFrame) out << "q\n0 0 0 RG 0.5 w " << rect << "S\nQ\n";
}
}
