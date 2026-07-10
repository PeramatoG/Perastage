#pragma once

#include "gdtf/editor/project_fixture_gdtf_apply_adapter.h"

namespace gui {

// Builds the GUI-hosted service bridge used by the non-GUI fixture adapter.
gdtf::ProjectFixtureGdtfApplyServices MakeFixtureGdtfApplyServices();

} // namespace gui
