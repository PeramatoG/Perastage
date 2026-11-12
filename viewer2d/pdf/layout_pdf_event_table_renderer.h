#pragma once
#include "layouts/EventTableComposition.h"
#include "viewer2dpdfexporter.h"
#include "pdf_draw_commands.h"
namespace layout_pdf_internal {
layouts::composition::EventTablePlan RenderEventTablePdf(std::ostringstream &out,
    const LayoutEventTableExportData &table, double pageHeight,
    double scaleX, double scaleY, const PdfFontCatalog &fonts,
    const FloatFormatter &formatter);
}
