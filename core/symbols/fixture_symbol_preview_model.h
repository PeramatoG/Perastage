#pragma once

#include "fixture_symbol_resolution.h"

struct FixtureSymbolPreviewModel {
  FixtureSymbolResolutionPurpose purpose = FixtureSymbolResolutionPurpose::StandardGdtf;
  FixtureSymbolResolution resolution;
  FixtureSymbolResource resource;
  bool available = false;
};

// Compares stored resources without allowing rendering fallbacks to impersonate originals.
FixtureSymbolPreviewModel BuildFixtureSymbolPreviewModel(
    const FixtureSymbolResourceInspection &inspection,
    FixtureSymbolResourceSet set, SymbolViewKind view);
