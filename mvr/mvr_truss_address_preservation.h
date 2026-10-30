/* This file is part of Perastage. */
#pragma once
#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
#include <filesystem>
#include <functional>
namespace tinyxml2 { class XMLElement; }
namespace mvr {
// Standard assignments always own their binding. Imported public definitions without
// auxiliary ownership also remain immutable; legacy native generation stays separate.
inline bool PreserveTrussAddressBinding(const Truss &truss) {
  return truss.dmxAddresses.has_value() || (!truss.gdtfSpec.empty() &&
      truss.perastageAuxGdtfArchivePath.empty() &&
      truss.sourceRepresentation != Truss::GeometryRepresentation::NativePerastage &&
      truss.sourceRepresentation != Truss::GeometryRepresentation::Unknown);
}
std::vector<MvrExportDiagnostic> CheckTrussAddressBindings(const MvrScene &scene);
std::vector<MvrExportDiagnostic> CheckSerializedTrussAddressBindings(
    const tinyxml2::XMLElement &root, const MvrScene &scene,
    const std::function<std::filesystem::path(const std::string &)> &resource);
}
