# Fixture GDTF derivative and project-symbol identity

## Separate identities and owners

`Manufacturer@FixtureType@Perastage.gdtf` is a Perastage derivative naming
convention, not an official GDTF semantic. The
[GDTF symbol ownership and mutation contract](gdtf_symbol_ownership_and_mutation_contract.md)
separates standards-compliant device definitions from project presentation.
GDTF 1.2 owns standard Top/Side/Front resources and official model offsets;
PSTG owns Perastage Top/Front/Side/Bottom symbols and user overrides. A project
symbol is not evidence that a GDTF is complete or Perastage-owned.

Core prepares intentional GDTF changes in private working storage, applies
standard mutations and revisions, canonicalizes, and atomically publishes the
derivative before rebinding fixtures. Valid external FixtureTypeID values are
preserved. Shared publication rejects invalid structure without replacing an
existing destination. Missing standard views may be completed when policy
permits; existing usable or unusable views are never automatically replaced.
Project-symbol Apply does not create or rewrite a GDTF derivative.

During a project GDTF edit, the successfully published project definition and
`Fixture.gdtfSpec` are authoritative. Library synchronization is secondary; its
failure is a warning and cannot roll back a valid project result. Successful
publication invalidates the parsed-SVG cache and triggers viewer refresh.
Private `.working` references are never attached to fixtures. Project internal
symbols remain independent from physical-property edits and derivative validation.

## Replacement identity

Imported source identity, selected replacement identity, project ownership, and
the user-facing fixture label remain separate. Within one import transaction,
an explicit GDTF Share RID plus exact compatible mode proves that different
source aliases chose the same replacement. Downloads are reused and affected
fixtures share one project reference. Similar names alone never prove equivalence.

Project symbol bindings are explicit fixture-UUID to definition-ID to
content-ID mappings, persisted by `core/symbols/project_fixture_symbols.*`.
Generated fallback and user override kinds, source FixtureTypeID/fingerprint,
generator/schema versions, view offsets, optional per-view archive/provenance
metadata, and exact SVG bytes are retained.
Newly inserted instances inherit only an unambiguous content identity shared
by all bound fixtures with the same nonempty exact source reference and mode;
conflicting overrides remain untouched and diagnostic. Identical bundles share
content-addressed payloads. Only definitions referenced
by the current scene are packaged. See the [resource contract](fixture_symbol_resource_contract.md)
for the archive layout and schema.

## Startup, persistence, and legacy input

Project Save serializes the scene and referenced project bundles through
`ProjectSession::ArchiveResource`. It does not generate symbols or wait for
preparation. Project symbols reside outside `scene.mvr` and are not exported as
private MVR files or embedded GDTF resources. Load restores exact project
presentation without requiring a global cache, original external GDTF, or a
matching generator version. Corrupt indexed bundles fail loading transactionally
before active scene/config callbacks, preserving the previous project.

`gdtf_derivative_contract_version = 1` remains the historical derivative migration
marker, separate from the versioned PSTG project-symbol schema. Projects without
that marker retain the bounded legacy migration over unique referenced GDTFs.
Historical numbered copies may be unified only when authoritative base content
and exact mode agree after removing narrowly recognized legacy symbol output.
Unsupported or ambiguous resources stay untouched and do not prevent Save.

Recognized legacy private symbols, Bottom extensions, and positively identified
old generated standard-location resources remain read-only inputs. On Save, before legacy GDTF reference consolidation, and before automatic
standard completion retargets references,
a compatible internal representation is materialized as project-owned
resources without rewriting the external source on open. Unknown authored SVGs
are not claimed by basename, generic editor metadata, or unrelated revisions.
Overwritten original manufacturer bytes require a clean authoritative source;
Perastage does not reconstruct them speculatively. Intentional derived publication
removes recognized private compatibility output through the shared Core policy.
Unchanged legacy input can remain embedded under the existing canonical MVR
preservation policy; the migration adds no new private files to that GDTF.

## Runtime preparation

The preparation service keys work by resolved physical GDTF path and exact
mode; duplicate requests coalesce and distinct modes remain distinct. A project
epoch rejects callbacks from replaced or closed projects. Each four-view
capture is one atomic GUI-thread operation. CPU processing and later project
application/standard publication are cooperative stages. Jobs recheck source
fingerprints before application.

Persisted project symbols are authoritative and are not regenerated merely
because the generator changed. Manual Apply stores a `UserOverride`; automatic
work stores `GeneratedFallback` only when an internal representation is needed.
Automatic standard completion separately consults the configured mutation policy
and never replaces existing resources. Source changes or explicit regeneration
must follow the product workflow rather than silently discarding an override.

Renderers continue to draw standard/geometry fallback immediately while work is
pending. Save, Load, MVR import/export, print, layout, and PDF do not wait for
queue completion. The runtime parsed-SVG cache remains disposable and independent
from project storage; project lifecycle transitions clear it, and physical
standard derivative replacement invalidates the changed resource path.

Project symbol tokens include immutable definition/content identity and
participate in live layout frame hashes. Apply and Undo/Redo invalidate layout
visual content. Saved layout cache validation includes the exact referenced
PSTG symbol resources together with packaged layout images, independently from
the `scene.mvr` fingerprint. Background PDF workers read an immutable captured
store through the scoped Core read context, preserving the selected definition
when the active project or its overrides change.
