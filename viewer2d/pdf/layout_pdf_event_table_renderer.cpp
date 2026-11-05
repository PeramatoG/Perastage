#include "layout_pdf_event_table_renderer.h"
#include "layout_pdf_page_mapping.h"
#include <algorithm>
namespace layout_pdf_internal {
layouts::composition::EventTablePlan RenderEventTablePdf(std::ostringstream &out,
    const LayoutEventTableExportData &table, double pageHeight,
    double scaleX, double scaleY, const PdfFontCatalog &fonts,
    const FloatFormatter &formatter) {
  using namespace layouts::composition;
  const typography::TtfFontMetrics missing;
  const auto plan = ComposeEventTable(
      {static_cast<double>(table.frame.width), static_cast<double>(table.frame.height), table.fields},
      {fonts.labelRegular ? fonts.labelRegular->metrics : missing,
       fonts.labelBold ? fonts.labelBold->metrics : missing});
  if (plan.clip.width <= 0 || plan.clip.height <= 0) return plan;
  ScopedDocumentPageMapping pageMapping(out, pageHeight, scaleX, scaleY, formatter);
  const auto x = [&](double local) { return formatter.Format(table.frame.x + local); };
  const auto y = [&](double local) { return formatter.Format(pageHeight - table.frame.y - local); };
  auto rect = [&](const Rect &r) {
    out << x(r.x) << ' ' << y(r.y + r.height) << ' '
        << formatter.Format(r.width) << ' ' << formatter.Format(r.height) << " re ";
  };
  out << "q\n"; rect(plan.clip); out << "W n\n1 1 1 rg "; rect(plan.clip); out << "f\n";
  for (const auto &row : plan.rows) {
    for (const auto *cell : {&row.label, &row.value}) {
      out << "q\n"; rect(cell->clip); out << "W n\n";
      std::string supported;
      for (const auto &glyph : cell->run.glyphs) supported += glyph.text;
      if (!supported.empty()) {
        out << "BT\n/" << (cell->role == TextRole::Bold ? "F4" : "F3") << ' '
            << formatter.Format(cell->run.style.size) << " Tf\n0.078431 0.078431 0.078431 rg\n"
            << x(cell->baseline.x) << ' ' << y(cell->baseline.y) << " Td\n("
            << EscapePdfLiteralBytes(EncodeWinAnsi(supported)) << ") Tj\nET\n";
      }
      out << "Q\n";
    }
  }
  out << "Q\n0 0 0 RG " << formatter.Format(plan.borderWidth) << " w ";
  rect(plan.clip); out << "S\n";
  return plan;
}
}
