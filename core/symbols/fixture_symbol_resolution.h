#pragma once

#include "fixture_symbol_resource_contract.h"

// Dedicated internal content does not fill standard Top/Side/Front requests.
enum class FixtureSymbolResolutionPurpose { StandardGdtf, InternalRendering };
enum class FixtureSymbolFallbackReason {
  None,
  SideCompatibility,
  LegacyResource,
  StandardRenderingResource,
  TopView,
  RuntimeGeometry,
};

struct FixtureSymbolResolution {
  SymbolViewKind requestedView = SymbolViewKind::Top;
  SymbolViewKind resolvedView = SymbolViewKind::Top;
  FixtureSymbolResourceSet resourceSet = FixtureSymbolResourceSet::StandardGdtf;
  FixtureSymbolProvenance provenance = FixtureSymbolProvenance::RuntimeFallback;
  std::string archivePath;
  bool exists = false;
  bool usable = false;
  double offsetXmm = 0.0;
  double offsetYmm = 0.0;
  bool usedViewFallback = false;
  FixtureSymbolFallbackReason fallbackReason = FixtureSymbolFallbackReason::None;
  std::string diagnostic;
};

// Pure selection over the existing inspection contract. Unusable runtime results
// instruct the caller to render geometry; they do not fabricate stored resources.
FixtureSymbolResolution ResolveFixtureSymbolView(
    const FixtureSymbolResourceInspection &inspection, SymbolViewKind requestedView,
    FixtureSymbolResolutionPurpose purpose);
