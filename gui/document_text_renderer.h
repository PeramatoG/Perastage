#pragma once
#include "typography/composition.h"
class wxGraphicsContext;
class wxGraphicsPath;
namespace typography { struct GlyphOutline; }
namespace gui {
void ReportDocumentTypographyDiagnostic(const std::string &diagnostic);
// Builds existing font contours at a resolved output baseline without relayout.
wxGraphicsPath BuildDocumentGlyphPath(wxGraphicsContext &graphics,
    const typography::GlyphOutline &outline, double baselineX,
    double baselineY, double size);
// Draw the existing typography engine's positioned glyphs at a page baseline.
void DrawComposedDocumentText(wxGraphicsContext &graphics,
    const typography::Composition &run, const typography::FontCatalog &catalog,
    double baselineX, double baselineY, double outputScale);
}
