#pragma once
#include "typography/composition.h"
class wxGraphicsContext;
namespace gui {
// Draw the existing typography engine's positioned glyphs at a page baseline.
void DrawComposedDocumentText(wxGraphicsContext &graphics,
    const typography::Composition &run, const typography::TtfFontMetrics &font,
    double baselineX, double baselineY, double outputScale);
}
