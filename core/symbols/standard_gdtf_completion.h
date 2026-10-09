#pragma once

#include "fixture_gdtf_derivative_publication.h"
#include "gdtf_mutation_policy.h"
#include "standard_gdtf_svg.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace symbols {

enum class StandardGdtfViewState { Missing, ExistingUsable, ExistingUnusable };

struct StandardGdtfViewInspection {
  SymbolViewKind viewKind = SymbolViewKind::Top;
  StandardGdtfViewState state = StandardGdtfViewState::Missing;
  std::string archivePath;
  std::string diagnostic;
};

struct StandardGdtfMutationResult {
  bool success = false;
  bool changed = false;
  std::filesystem::path publishedPath;
  std::vector<StandardGdtfViewInspection> views;
  std::vector<std::string> diagnostics;
  std::string errorMessage;
};

// Library publishers additionally register the validated derivative ownership.
using StandardGdtfPublication = std::function<bool(
    const fixture_gdtf::PreparedDerivative &, std::string &)>;

// Inspects exactly Model/@File resources, without compatibility name guessing.
// Empty modelFile selects Main, or the first Model when Main is absent.
StandardGdtfMutationResult InspectStandardGdtfViews(
    const std::filesystem::path &sourcePath, std::string modelFile = "");

// Completes missing views only. Existing unusable resources remain diagnostics.
// No-op requests discard the prepared copy and never append a revision.
StandardGdtfMutationResult CompleteStandardGdtfViews(
    const std::filesystem::path &sourcePath,
    const fixture_gdtf::PreparedDerivative &prepared,
    const std::vector<StandardGdtfSvgCandidate> &candidates,
    gdtf::MutationPolicy policy, std::string modelFile = "",
    const StandardGdtfPublication &publish = {});

enum class StandardGdtfReplacementIntent { RepairInvalid, ReplaceExisting };

// This separate operation must be invoked only for explicit user intent.
StandardGdtfMutationResult ReplaceStandardGdtfViews(
    const std::filesystem::path &sourcePath,
    const fixture_gdtf::PreparedDerivative &prepared,
    const std::vector<StandardGdtfSvgCandidate> &candidates,
    StandardGdtfReplacementIntent intent, std::string modelFile = "",
    const StandardGdtfPublication &publish = {});

} // namespace symbols
