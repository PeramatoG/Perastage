#include "fixture_symbol_preview_model.h"

FixtureSymbolPreviewModel BuildFixtureSymbolPreviewModel(
    const FixtureSymbolResourceInspection &inspection,
    FixtureSymbolResourceSet set, SymbolViewKind view) {
  FixtureSymbolPreviewModel model;
  model.purpose = set == FixtureSymbolResourceSet::StandardGdtf
      ? FixtureSymbolResolutionPurpose::StandardGdtf
      : FixtureSymbolResolutionPurpose::InternalRendering;
  model.resource.viewKind = view;
  model.resource.resourceSet = set;
  model.resource.standardGdtf = set == FixtureSymbolResourceSet::StandardGdtf;
  const auto *resource = set == FixtureSymbolResourceSet::StandardGdtf
      ? inspection.FindStandardView(view) : inspection.FindPerastageView(view);
  if (resource)
    model.resource = *resource;
  model.resolution = ResolveFixtureSymbolView(inspection, view, model.purpose);
  const auto storedView = view == SymbolViewKind::Right ? SymbolViewKind::Left : view;
  model.available = model.resolution.exists && model.resolution.usable &&
      model.resolution.resourceSet == set &&
      model.resolution.resolvedView == storedView &&
      !model.resolution.usedViewFallback &&
      !(set == FixtureSymbolResourceSet::StandardGdtf && view == SymbolViewKind::Bottom);
  if (model.available) {
    model.resource.archivePath = model.resolution.archivePath;
    model.resource.provenance = model.resolution.provenance;
    model.resource.exists = true;
    model.resource.usable = true;
    model.resource.diagnostic = model.resolution.diagnostic;
  } else if (model.resource.diagnostic.empty()) {
    model.resource.diagnostic = inspection.diagnostic;
    if (model.resource.diagnostic.empty())
      model.resource.diagnostic = "No usable stored resource in the requested set and view.";
  }
  return model;
}
