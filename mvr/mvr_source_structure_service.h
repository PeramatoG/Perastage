#pragma once
#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
#include "mvr_export_options.h"
#include "mvr_standard_field_projection.h"
#include <functional>
namespace tinyxml2 {
class XMLElement;
class XMLDocument;
} // namespace tinyxml2
namespace mvr {
void IndexSourceStructure(MvrSourceStructure &source);
void CaptureSourceStructure(
    tinyxml2::XMLElement &sceneElement, MvrScene &scene,
    const std::function<std::string(const std::string &)> &remapResource);
const MvrSourceNode *FindSourceNode(const MvrScene &scene,
                                    const std::string &uuid);
bool SourceOwnsGeometry(const MvrScene &scene, const std::string &uuid);
bool SourceNodeSurvives(const MvrScene &scene, const MvrSourceNode &node);
bool CanRestoreSourceIssue(const MvrScene &scene,
                           const MvrPreservationIssue &issue);
std::vector<MvrExportDiagnostic> CheckSourceStructure(const MvrScene &scene);
enum class SourceResourceKind { Model, Gdtf, Media };
using SourceResource = std::function<std::string(
    const std::string &owner, const std::string &path, SourceResourceKind)>;
StandardFieldProjectionResult
ProjectSourceStructure(tinyxml2::XMLDocument &document, const MvrScene &scene,
                       const SourceResource &resource,
                       const MvrExportOptions &options = {});
void PrepareSourceStructureEdits(MvrScene &scene);
void ReconcileSourceStructure(
    MvrScene &scene,
    const std::function<std::string(const std::string &)> &uuid,
    const std::function<std::string(const std::string &)> &resource);
std::vector<std::string> SourceStructureIdentities(const MvrScene &scene);
// Expands only the display projection. Canonical instances/definitions stay
// typed.
std::vector<SymdefGeometry> ExpandSymdefGeometry(const MvrScene &scene,
                                                 const std::string &uuid);
} // namespace mvr
