#pragma once
#include "viewer2dpdfexporter.h"
#include "pdf_objects.h"
namespace layout_pdf_internal {
class PdfUnicodeFonts;
void RenderLayoutTextPdf(std::ostringstream &, const LayoutTextExportData &,
    double pageHeight, double scaleX, double scaleY,
    PdfUnicodeFonts &, const FloatFormatter &);
}
