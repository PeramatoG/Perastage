#pragma once

#include "mvr_export_diagnostic.h"

#include <vector>

class wxWindow;

// Presents one bounded, aggregated result for an interactive MVR export.
void ShowMvrExportResult(wxWindow *parent, bool exported,
                         const std::vector<MvrExportDiagnostic> &diagnostics);
