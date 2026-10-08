#include "fixture_symbol_resolution.h"

namespace {

SymbolViewKind StoredView(SymbolViewKind view) {
  return view == SymbolViewKind::Right ? SymbolViewKind::Left : view;
}

const FixtureSymbolResource *FindCandidate(
    const FixtureSymbolResourceInspection &inspection, SymbolViewKind view,
    FixtureSymbolResourceSet set, FixtureSymbolProvenance provenance) {
  const auto eligible = [&](const FixtureSymbolResource &resource) {
    return resource.viewKind == view && resource.resourceSet == set &&
           resource.provenance == provenance && resource.exists && resource.usable;
  };
  const auto &candidates = set == FixtureSymbolResourceSet::StandardGdtf
                               ? inspection.standardResources
                               : inspection.perastageResources;
  for (const auto &resource : candidates) {
    if (eligible(resource))
      return &resource;
  }
  const auto *primary = set == FixtureSymbolResourceSet::StandardGdtf
                            ? inspection.FindStandardView(view)
                            : inspection.FindPerastageView(view);
  return primary && eligible(*primary) ? primary : nullptr;
}

const FixtureSymbolResource *StandardCandidate(
    const FixtureSymbolResourceInspection &inspection, SymbolViewKind view) {
  const auto *authored = FindCandidate(inspection, view,
      FixtureSymbolResourceSet::StandardGdtf, FixtureSymbolProvenance::AuthoredGdtf);
  return authored ? authored : FindCandidate(inspection, view,
      FixtureSymbolResourceSet::StandardGdtf,
      FixtureSymbolProvenance::GeneratedPerastage);
}

const FixtureSymbolResource *SelectView(
    const FixtureSymbolResourceInspection &inspection, SymbolViewKind view,
    FixtureSymbolResolutionPurpose purpose, FixtureSymbolFallbackReason &reason) {
  const bool internal = purpose == FixtureSymbolResolutionPurpose::InternalRendering;
  if (!internal && view != SymbolViewKind::Bottom) {
    if (const auto *resource = StandardCandidate(inspection, view))
      return resource;
  }
  if (internal || view == SymbolViewKind::Bottom) {
    if (const auto *resource = FindCandidate(inspection, view,
        FixtureSymbolResourceSet::Perastage,
        FixtureSymbolProvenance::GeneratedPerastage))
      return resource;
  }
  if (const auto *resource = FindCandidate(inspection, view,
      FixtureSymbolResourceSet::Perastage, FixtureSymbolProvenance::LegacyPerastage)) {
    reason = FixtureSymbolFallbackReason::LegacyResource;
    return resource;
  }
  if (internal && view != SymbolViewKind::Bottom) {
    if (const auto *resource = StandardCandidate(inspection, view)) {
      reason = FixtureSymbolFallbackReason::StandardRenderingResource;
      return resource;
    }
  }
  return nullptr;
}

void AppendDiagnostic(std::string &text, const std::string &message) {
  if (message.empty())
    return;
  if (!text.empty())
    text += " ";
  text += message;
}

// Report rejected resources as well as the reason for selecting a fallback.
std::string ViewDiagnostics(const FixtureSymbolResourceInspection &inspection,
                            SymbolViewKind view) {
  std::string diagnostic;
  const auto append = [&](const FixtureSymbolResource &resource) {
    if (resource.viewKind == view && resource.exists && !resource.usable)
      AppendDiagnostic(diagnostic, resource.archivePath + ": " + resource.diagnostic);
  };
  for (const auto &resource : inspection.standardResources)
    append(resource);
  for (const auto &resource : inspection.perastageResources)
    append(resource);
  return diagnostic;
}

} // namespace

FixtureSymbolResolution ResolveFixtureSymbolView(
    const FixtureSymbolResourceInspection &inspection, SymbolViewKind requestedView,
    FixtureSymbolResolutionPurpose purpose) {
  FixtureSymbolResolution result;
  result.requestedView = requestedView;
  result.resolvedView = StoredView(requestedView);
  result.resourceSet = purpose == FixtureSymbolResolutionPurpose::StandardGdtf
                           ? FixtureSymbolResourceSet::StandardGdtf
                           : FixtureSymbolResourceSet::Perastage;
  result.diagnostic = ViewDiagnostics(inspection, result.resolvedView);
  const auto *resource = SelectView(inspection, result.resolvedView, purpose,
                                    result.fallbackReason);
  if (!resource && purpose == FixtureSymbolResolutionPurpose::InternalRendering &&
      result.resolvedView != SymbolViewKind::Top) {
    resource = SelectView(inspection, SymbolViewKind::Top, purpose, result.fallbackReason);
    if (resource) {
      result.usedViewFallback = true;
      result.fallbackReason = FixtureSymbolFallbackReason::TopView;
      AppendDiagnostic(result.diagnostic, "Requested SVG view is unavailable; using Top.");
    }
  }
  if (!resource) {
    result.resolvedView = requestedView;
    result.fallbackReason = FixtureSymbolFallbackReason::RuntimeGeometry;
    AppendDiagnostic(result.diagnostic, inspection.diagnostic);
    AppendDiagnostic(result.diagnostic,
                     "No usable resource for this request; runtime geometry is required.");
    return result;
  }
  result.resolvedView = resource->viewKind;
  result.resourceSet = resource->resourceSet;
  result.provenance = resource->provenance;
  result.archivePath = resource->archivePath;
  result.exists = resource->exists;
  result.usable = resource->usable;
  result.offsetXmm = resource->offsetXmm;
  result.offsetYmm = resource->offsetYmm;
  AppendDiagnostic(result.diagnostic, resource->diagnostic);
  if (result.fallbackReason == FixtureSymbolFallbackReason::LegacyResource)
    AppendDiagnostic(result.diagnostic, "Using a recognized legacy Perastage resource.");
  else if (result.fallbackReason == FixtureSymbolFallbackReason::StandardRenderingResource)
    AppendDiagnostic(result.diagnostic, "Using a standard GDTF resource for internal rendering.");
  if (requestedView == SymbolViewKind::Right && result.resolvedView == SymbolViewKind::Left) {
    if (result.fallbackReason == FixtureSymbolFallbackReason::None)
      result.fallbackReason = FixtureSymbolFallbackReason::SideCompatibility;
    AppendDiagnostic(result.diagnostic, "Right uses the compatible Side resource.");
  }
  return result;
}
