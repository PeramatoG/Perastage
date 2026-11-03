#pragma once

#include "layouts/LegendContent.h"
#include "layouts/LegendComposition.h"
#include "symbolcache.h"
#include <wx/dc.h>
#include <wx/image.h>

namespace gui {
layouts::composition::TextMeasurement MeasureLegendPreviewText(
    wxDC &dc, std::string_view text, double size,
    layouts::composition::TextRole role);

wxImage RenderLegendImage(const wxSize &size, const wxSize &logicalSize,
    double renderZoom, const std::vector<layouts::LegendResolvedItem> &items,
    const layouts::LayoutLegendDefinition &legend,
    const SymbolDefinitionSnapshot *symbols);
} // namespace gui
