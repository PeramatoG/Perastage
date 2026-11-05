#pragma once
#include "layouts/EventTableComposition.h"
#include "layouts/LayoutCollection.h"
#include <wx/image.h>
#include <wx/gdicmn.h>
namespace gui {
wxImage RenderEventTableImage(const wxSize &size, double zoom,
    const layouts::LayoutEventTableDefinition &table,
    layouts::composition::EventTablePlan *composedPlan = nullptr);
}
