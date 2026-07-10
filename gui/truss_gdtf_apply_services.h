#pragma once

#include "gdtf/editor/project_truss_gdtf_apply_adapter.h"

namespace gui {

// Builds the GUI-hosted service bridge used by the non-GUI truss adapter.
gdtf::ProjectTrussGdtfApplyServices MakeTrussGdtfApplyServices();

} // namespace gui
