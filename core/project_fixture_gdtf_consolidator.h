#pragma once

#include <string>
#include <vector>

class MvrScene;

namespace project_gdtf {

struct Rebind {
  std::string fixtureUuid;
  std::string oldGdtfSpec;
  std::string newGdtfSpec;
};

struct ConsolidationGroup {
  std::string baseFingerprint;
  std::string mode;
  std::string survivorGdtfSpec;
  std::vector<std::string> candidateGdtfSpecs;
  std::vector<Rebind> rebindings;
};

struct ConsolidationPlan {
  std::vector<ConsolidationGroup> groups;
  std::vector<std::string> diagnostics;
  bool complete = true;
};

// Computes a fingerprint that excludes only recognized Perastage symbol output.
std::string ComputeBaseGdtfFingerprint(const std::string &path,
                                       std::string &errorMessage);

// Builds a deterministic and inspectable project-resource consolidation plan.
ConsolidationPlan BuildConsolidationPlan(const MvrScene &scene);

// Applies a fully validated plan atomically to fixture GDTF references.
bool ApplyConsolidationPlan(MvrScene &scene, const ConsolidationPlan &plan,
                            std::string &errorMessage);

} // namespace project_gdtf
