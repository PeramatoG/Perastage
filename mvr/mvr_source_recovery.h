#pragma once
#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
#include <vector>
namespace mvr {
// Operates only on an export snapshot. Never allocates replacement model IDs.
std::vector<MvrExportDiagnostic> RecoverRetainedSource(MvrScene &scene);
} // namespace mvr
