/* This file is part of Perastage. */
#pragma once
#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
#include <filesystem>
#include <functional>
namespace tinyxml2 { class XMLElement; }
namespace mvr {
bool PreserveTrussAddressBinding(const Truss &truss);
std::vector<MvrExportDiagnostic> CheckTrussAddressBindings(const MvrScene &scene);
std::vector<MvrExportDiagnostic> CheckSerializedTrussAddressBindings(
    const tinyxml2::XMLElement &root, const MvrScene &scene,
    const std::function<std::filesystem::path(const std::string &)> &resource);
}
