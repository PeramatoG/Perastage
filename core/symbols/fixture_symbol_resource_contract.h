#pragma once

#include "symbol_view_kind.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

inline constexpr char kPerastageSymbolVersionAttribute[] =
    "data-perastage-symbol-version";
inline constexpr char kPerastageSymbolResourceSetAttribute[] =
    "data-perastage-resource-set";
inline constexpr char kStandardGdtfSymbolResourceSetValue[] = "standard-gdtf";
inline constexpr char kPerastageSymbolOffsetXAttribute[] =
    "data-perastage-offset-x-mm";
inline constexpr char kPerastageSymbolOffsetYAttribute[] =
    "data-perastage-offset-y-mm";
inline constexpr int kCurrentPerastageSymbolResourceVersion = 1;

enum class FixtureSymbolResourceSet {
  StandardGdtf,
  InternalRendering,
  Perastage = InternalRendering, // Legacy source-compatible spelling.
};

enum class FixtureSymbolProvenance {
  None,
  AuthoredGdtf,
  GeneratedPerastage,
  ProjectUserOverride,
  LegacyPerastage,
  RuntimeFallback,
};

struct FixtureSymbolResource {
  SymbolViewKind viewKind = SymbolViewKind::Top;
  std::string archivePath;
  bool exists = false;
  bool usable = false;
  FixtureSymbolProvenance provenance = FixtureSymbolProvenance::None;
  FixtureSymbolResourceSet resourceSet = FixtureSymbolResourceSet::StandardGdtf;
  bool standardGdtf = true;
  std::string diagnostic;
  double offsetXmm = 0.0;
  double offsetYmm = 0.0;

  bool PerastageOwned() const {
    return provenance == FixtureSymbolProvenance::GeneratedPerastage ||
           provenance == FixtureSymbolProvenance::ProjectUserOverride ||
           provenance == FixtureSymbolProvenance::LegacyPerastage;
  }
};

struct FixtureSymbolResourceInspection {
  // GDTF Side is represented by the existing Left/Right rendering orientation.
  std::array<FixtureSymbolResource, 3> standardViews = {{
      {.viewKind = SymbolViewKind::Top},
      {.viewKind = SymbolViewKind::Front},
      {.viewKind = SymbolViewKind::Left},
  }};
  std::array<FixtureSymbolResource, 4> perastageViews = {{
      {.viewKind = SymbolViewKind::Top,
       .resourceSet = FixtureSymbolResourceSet::Perastage,
       .standardGdtf = false},
      {.viewKind = SymbolViewKind::Bottom,
       .resourceSet = FixtureSymbolResourceSet::Perastage,
       .standardGdtf = false},
      {.viewKind = SymbolViewKind::Front,
       .resourceSet = FixtureSymbolResourceSet::Perastage,
       .standardGdtf = false},
      {.viewKind = SymbolViewKind::Left,
       .resourceSet = FixtureSymbolResourceSet::Perastage,
       .standardGdtf = false},
  }};
  std::string modelSvgBasename;
  // Includes legacy alternatives even when a dedicated resource is preferred.
  std::vector<FixtureSymbolResource> perastageResources;
  // Retains compatible standard candidates for explicit per-view resolution.
  std::vector<FixtureSymbolResource> standardResources;
  bool standardViewsUsable = false;
  bool perastageViewsUsable = false;
  std::string diagnostic;

  const FixtureSymbolResource *FindStandardView(SymbolViewKind view) const {
    if (view == SymbolViewKind::Right)
      view = SymbolViewKind::Left;
    for (const auto &resource : standardViews) {
      if (resource.viewKind == view)
        return &resource;
    }
    return nullptr;
  }

  const FixtureSymbolResource *FindPerastageView(SymbolViewKind view) const {
    if (view == SymbolViewKind::Right)
      view = SymbolViewKind::Left;
    for (const auto &resource : perastageViews) {
      if (resource.viewKind == view)
        return &resource;
    }
    return nullptr;
  }
};

// Inspects stored resources without rendering fallback or changing the source.
// The standard three-view and internal four-view sets remain independent.
bool InspectFixtureSymbolResources(
    const std::string &gdtfPath,
    FixtureSymbolResourceInspection &inspection);

// Legacy read-only lookup. New internal symbols belong to PSTG project storage.
std::string BuildPerastageFixtureSymbolPath(std::string_view modelSvgBasename,
                                           SymbolViewKind view);

// Recognizes only the exact symbol actions emitted by the legacy writer.
bool IsPerastageFixtureSymbolRevision(std::string_view modifiedBy,
                                     std::string_view text);
bool IsPerastageFixtureSymbolRevisionForView(std::string_view modifiedBy,
                                            std::string_view text,
                                            SymbolViewKind view);

// Recognizes exact standard completion/explicit replacement audit actions.
// This establishes derivative mutation provenance, not project-symbol ownership.
bool IsPerastageStandardSvgMutationRevision(std::string_view modifiedBy,
                                           std::string_view text);
bool IsPerastageStandardSvgMutationRevisionForView(std::string_view modifiedBy,
                                                  std::string_view text,
                                                  SymbolViewKind view);

// Positive publication cleanup supersedes legacy ownership at official paths.
// SVG ownership markers themselves remain authoritative compatibility evidence.
bool IsPerastageStandardSvgCleanupRevision(std::string_view modifiedBy,
                                          std::string_view text);
