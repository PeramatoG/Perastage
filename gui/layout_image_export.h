#pragma once
#include "layouts/PageOutputMapping.h"
#include "viewer2dpdfexporter.h"
#include <wx/gdicmn.h>
namespace gui {
wxSize LayoutImageRasterSize(const layouts::Layout2DViewFrame &,
    layouts::composition::PageOutputMapping);
std::optional<LayoutImageExportData> BuildLayoutImageExportData(
    const layouts::LayoutImageDefinition &, layouts::composition::PageOutputMapping,
    std::string *diagnostic = nullptr);
}
