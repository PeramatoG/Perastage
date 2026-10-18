#pragma once

#include "mvrscene.h"
#include "mvr_export_diagnostic.h"

namespace tinyxml2 { class XMLElement; }

namespace mvr {
// Records known source losses before normalization. This is not a validator.
void RecordStandardPreservationRisks(tinyxml2::XMLElement &root, MvrScene &scene);

// Checks durable evidence and focus dependencies before any export recovery.
std::vector<MvrExportDiagnostic> CheckStandardPreservation(const MvrScene &scene);
std::vector<MvrExportDiagnostic> CheckSerializedFocus(tinyxml2::XMLElement &scene);
} // namespace mvr
