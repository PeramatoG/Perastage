/* This file is part of Perastage. */
#pragma once
#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
#include "mvr_export_options.h"
#include <functional>
#include <filesystem>
namespace tinyxml2 { class XMLElement; }
namespace mvr {
bool SymdefMatrixPreservesSource(const std::string &text, const Matrix &projection);
void ReadMvrSymdefs(tinyxml2::XMLElement &sceneElement, MvrScene &scene,
                   const std::function<std::string(const std::string &)> &remapResource);
std::vector<MvrExportDiagnostic> CheckMvrSymdefs(const MvrScene &scene);
std::vector<MvrExportDiagnostic> CheckSerializedSymdefs(
    tinyxml2::XMLElement &sceneElement, const MvrScene &scene, const MvrExportOptions &options,
    const std::function<std::filesystem::path(const std::string &)> &resourceLookup);
std::filesystem::path ResolveSymdefResource(const MvrScene &scene, const std::string &file);
std::string SymdefResourceProblem(const MvrScene &scene, const std::string &file);
} // namespace mvr
