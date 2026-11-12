#pragma once
#include "mvrscene.h"
#include "mvr_export_diagnostic.h"
#include <filesystem>
#include <vector>
namespace tinyxml2 {
class XMLDocument;
}
namespace mvridentity {
struct RecoveryResult;
}
namespace mvr::publication {
void ReconcileIdentityReferences(MvrScene &,
                                 const mvridentity::RecoveryResult &);
std::string PlanningNumber(float);
using Findings = std::vector<MvrExportDiagnostic>;
bool FailedResourceReference(const MvrScene &, const std::string &owner,
                             const std::string &reference);
void Note(
    Findings &, const std::string &type, const std::string &id,
    const std::string &field, const std::string &action,
    const std::string &before,
    MvrExportDiagnosticImpact impact = MvrExportDiagnosticImpact::DataOmitted);
void RepairSourceIdentities(MvrScene &, Findings &);
void RepairSourceHierarchy(MvrScene &, Findings &);
void RepairSourceClasses(MvrScene &, Findings &);
void RepairNative(MvrScene &, Findings &);
void RepairSymbols(MvrScene &, Findings &);
void RepairResources(MvrScene &, const std::filesystem::path &fallback,
                     Findings &);
void RepairSource(MvrScene &, Findings &);
void AppendDisclosure(tinyxml2::XMLDocument &, const Findings &,
                      const std::string &sourceBasePath = {});
void RetainDisclosure(MvrScene &, const Findings &);
} // namespace mvr::publication
