#include "layout_event_table_renderer.h"
#include "document_text_renderer.h"
#include "typography/font_resources.h"
#include "projectutils.h"
#include "logger.h"
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>

namespace gui {
wxImage RenderEventTableImage(const wxSize &size, double zoom,
    const layouts::LayoutEventTableDefinition &table,
    layouts::composition::EventTablePlan *composedPlan) {
  using namespace layouts::composition;
  if (size.x <= 0 || size.y <= 0 || !std::isfinite(zoom) || zoom <= 0) return {};
  const typography::FontPair fonts(ProjectUtils::GetResourceRoot(), table.fontFamily);
  const auto plan = ComposeEventTable(
      {static_cast<double>(table.frame.width), static_cast<double>(table.frame.height), table.fields},
      {fonts.regular, fonts.bold, fonts.catalog.get(), table.fontFamily});
  if (composedPlan) *composedPlan = plan;
  for (const auto &message : plan.diagnostics) ReportDocumentTypographyDiagnostic(message);
  wxBitmap bitmap(size.x, size.y, 32);
  wxMemoryDC memory(bitmap);
  wxGCDC dc(memory);
  dc.SetBackground(*wxWHITE_BRUSH); dc.Clear();
  if (auto *gc = dc.GetGraphicsContext()) {
    gc->PushState();
    gc->Clip(0, 0, plan.clip.width * zoom, plan.clip.height * zoom);
    gc->SetBrush(wxBrush(wxColour(20,20,20)));
    gc->SetPen(*wxTRANSPARENT_PEN);
    for (const auto &row : plan.rows) {
      for (const auto *cell : {&row.label, &row.value}) {
        if (cell->clip.width <= 0 || cell->clip.height <= 0) continue;
        gc->PushState();
        gc->Clip(cell->clip.x * zoom, cell->clip.y * zoom,
                 cell->clip.width * zoom, cell->clip.height * zoom);
        DrawComposedDocumentText(*gc, cell->run,
            *fonts.catalog,
            cell->baseline.x, cell->baseline.y, zoom);
        gc->PopState();
      }
    }
    gc->PopState();
    gc->PushState(); gc->Scale(zoom, zoom);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(*wxBLACK).Width(plan.borderWidth)));
    gc->DrawRectangle(0, 0, plan.clip.width, plan.clip.height);
    gc->PopState();
  }
  return bitmap.ConvertToImage();
}
}
