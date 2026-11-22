#include "layout_pdf_event_table_renderer.h"
#include "pdf_unicode_fonts.h"
#include "layout_pdf_page_mapping.h"
#include <algorithm>
namespace layout_pdf_internal {
layouts::composition::EventTablePlan RenderEventTablePdf(std::ostringstream &out,
    const LayoutEventTableExportData &table, double pageHeight,
    double scaleX, double scaleY, PdfUnicodeFonts &ink,
    const FloatFormatter &formatter) {
  using namespace layouts::composition;
  const typography::TtfFontMetrics empty;
  const auto plan = ComposeEventTable(
      {static_cast<double>(table.frame.width), static_cast<double>(table.frame.height), table.fields},
      {empty, empty, &ink.Catalog(), table.fontFamily, typography::FontUsage::Pdf});
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
      out << "0.078431 0.078431 0.078431 rg\n";
      ink.AppendRun(out, formatter, cell->run, table.frame.x + cell->baseline.x,
                    pageHeight - table.frame.y - cell->baseline.y);
      out << "Q\n";
    }
  }
  out << "Q\n0 0 0 RG " << formatter.Format(plan.borderWidth) << " w ";
  rect(plan.clip); out << "S\n";
  return plan;
}
}
