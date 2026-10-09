# GDTF Symbol Ownership and Mutation Contract

## Status

This document defines the Perastage architecture and policy for fixture symbols, GDTF mutation, project persistence, and standards-compliant derivative publication.

It supersedes the previous assumption that Perastage-owned fixture symbols should be stored inside GDTF archives under a private `perastage/symbols/` namespace.

The contract is intended to guide implementation for Perastage 1.7.0 and later.

## Normative baseline

Perastage follows the current official GDTF and MVR specifications published by the MVR Development Group.

At the time this contract was written:

- GDTF current DIN SPEC: DIN SPEC 15800:2022-02.
- MVR current DIN SPEC: DIN SPEC 15801:2023-12.
- The official `main` specification is the publication baseline.
- Approved future changes from the official `next` specification may be anticipated internally, but Perastage must not emit resources or XML that are not valid for the GDTF version being written.

For GDTF 1.2, the standard model SVG views are:

- Top: `models/svg/<Model.File>.svg`
- Side: `models/svg_side/<Model.File>.svg`
- Front: `models/svg_front/<Model.File>.svg`

Their offsets belong to the corresponding standard `<Model>` attributes in `description.xml`.

Bottom is not a standard GDTF 1.2 model SVG view and must not be emitted as a standard GDTF 1.2 resource. Perastage may support Bottom internally. If a later supported GDTF version standardizes Bottom, the GDTF serializer may emit it only when targeting that version and according to that version's exact rules.


## Official references

- GDTF specification: https://github.com/mvrdevelopment/spec/blob/main/gdtf-spec.md
- MVR specification: https://github.com/mvrdevelopment/spec/blob/main/mvr-spec.md
- Specification repository/version policy: https://github.com/mvrdevelopment/spec/blob/main/README.md
- Approved future GDTF/MVR changes are maintained on the official `next` branch and must not be emitted by Perastage unless the written file declares and supports the corresponding version.

## Core principle

Perastage separates exchange-standard data from application-specific project data.

- **GDTF** owns standards-compliant device-definition data and resources.
- **MVR** owns standards-compliant scene exchange data and embedded GDTF resources.
- **PSTG** owns Perastage-specific project resources and behavior.
- **Caches** are disposable acceleration only and must never be the sole owner of user data.

Perastage may complete, correct, or edit a GDTF when the modification is intentional, standards-compliant, traceable, and published as a derived GDTF. Perastage-specific resources must not be embedded in GDTF or MVR merely because Perastage needs them internally.

## Import and publication philosophy

Perastage is permissive when importing and strict when publishing.

Reading a GDTF or MVR must not mutate the external source merely because it was opened or inspected.

When Perastage intentionally improves or edits a GDTF, it should normally publish a derived file using the existing `@Perastage` naming policy rather than overwrite the external source. The original source remains available when practical.

A Perastage-published GDTF must:

- use only resources and XML valid for its declared GDTF version;
- pass the shared canonicalization/validation boundary;
- preserve unrelated valid authored data and resources;
- append a standard GDTF `<Revision>` for every effective intentional mutation;
- avoid custom Perastage XML nodes or private archive namespaces;
- be published atomically through the shared Core publication path.

When an MVR contains an unchanged GDTF, Perastage should preserve that GDTF. When Perastage intentionally changes a GDTF used by the scene, the exported MVR may contain the standards-compliant derived GDTF and the GDTF must carry the corresponding standard revision history.

## Two independent symbol products

Perastage generates two distinct products from shared geometry/vectorization infrastructure.

### 1. Perastage project symbol

Purpose:

- Perastage layout rendering and printing;
- application-specific visual conventions;
- user-created or user-approved symbol overrides;
- current four-view representation, including Bottom;
- future Perastage-specific views, labels, styles, or metadata.

Ownership:

- PSTG project resources.

It is not a GDTF resource and must not be written to a GDTF archive.

Perastage project symbols may use Perastage-specific metadata because they are owned by the PSTG format.

### 2. Standard GDTF SVG resource

Purpose:

- standards-compliant completion or explicit replacement of GDTF model SVG resources.

Ownership:

- derived GDTF.

It must be serialized according to the exact GDTF version being targeted.

For GDTF 1.2 this means the official Top, Side, and Front paths based on `Model/@File`, with offsets written through the standard model offset attributes.

A standard GDTF SVG must not contain Perastage-private ownership markers such as `data-perastage-*`. Provenance belongs in the GDTF standard revision history and, if needed for project behavior, in PSTG metadata.

The Perastage project-symbol serializer and GDTF SVG serializer may share geometry/vectorization code, but they are separate output targets with separate validation rules.

## Standard resource mutation rules

Perastage distinguishes **missing**, **invalid**, and **existing valid** standard resources.

### Missing standard resource

If a standard resource required or supported by the targeted GDTF version is absent and Perastage can generate a valid replacement from trusted available geometry, Perastage may complete the missing resource automatically when the active GDTF mutation policy allows completion.

Examples for GDTF 1.2:

- missing Top SVG;
- missing Side SVG;
- missing Front SVG.

The resulting GDTF is a derived `@Perastage` GDTF and receives a standard revision describing the views that were added.

### Existing valid standard resource

Perastage must never automatically replace an existing valid authored standard SVG merely because the generated Perastage representation is preferred visually.

Replacement requires an explicit user action.

The UI should present the authored resource and generated candidate separately where practical before replacement.

### Existing invalid or unusable standard resource

Perastage must not silently overwrite an existing resource solely because Perastage cannot parse or render it.

The resource remains distinguishable from a missing resource.

Perastage may offer an explicit repair/replace operation. If the user approves it, the derived GDTF receives the replacement and a standard revision that clearly describes the repair.

## Other GDTF data completion and editing

The same principles apply beyond SVG symbols.

Perastage may add missing or intentionally edit standard data such as supported physical properties when:

- the value comes from explicit user input, a trusted authoritative source, or an unambiguous standards-defined derivation;
- the target field is valid for the GDTF version being written;
- the change is applied through the shared Core mutation/canonicalization path;
- an effective mutation receives a standard GDTF revision.

Perastage must not invent physical or semantic fixture data merely to make a file appear complete.

Automatic completion may add previously absent data. Replacing an existing meaningful value requires explicit user intent, for example through an edit operation.

## Revision and provenance policy

Every effective intentional GDTF mutation must be represented through the standard GDTF revision mechanism.

Perastage uses the shared `GdtfMutationAudit` service and standard `<FixtureType>/<Revisions>/<Revision>` entries.

Revision text should describe the effective operation, for example:

- `Added missing standard SVG Side and Front views`
- `Replaced standard SVG Front view after explicit user repair`
- `Added missing Weight and PowerConsumption`
- `Updated Weight from 22 kg to 23.5 kg`

`ModifiedBy` identifies Perastage and its application version through the existing shared helper.

Repeated no-op publication must not create a new revision.

Perastage must not reintroduce custom `PerastageMutationAudit` XML or any replacement private XML audit schema.

## Derived GDTF identity

Perastage should retain the existing standards-compatible derivative naming policy based on the GDTF archive filename structure, using `@Perastage` as the optional comment where appropriate.

Conceptually:

`Manufacturer@FixtureType@Perastage.gdtf`

A derived GDTF must preserve the fixture identity required by the standard unless a specific repair policy explicitly and safely repairs a Perastage-owned placeholder identifier.

A valid external FixtureTypeID must not be changed merely because a derivative is produced.

## PSTG symbol persistence

Perastage project symbols move out of GDTF and into PSTG-owned resources.

The project must remain self-contained: reopening a saved PSTG must reproduce the same user-visible symbol representation without requiring the original external GDTF, a global symbol library, or regeneration by a newer Perastage version.

A recommended project resource layout is:

```text
project.pstg
  config.json
  scene.mvr
  resources/
    fixture_symbols/
      <content-id>/
        manifest.json
        top.svg
        front.svg
        side.svg
        bottom.svg
```

The exact internal schema may evolve, but it must be versioned and owned by Core rather than by GUI code.

### Symbol bundle requirements

A persisted project-symbol bundle must distinguish at least:

- generated fallback symbols;
- explicit user overrides;
- generator/schema version;
- source fixture identity when available;
- source GDTF semantic/content fingerprint when relevant;
- available views;
- Perastage-specific offsets/metadata required to reproduce rendering.

The resource payload should be content-addressed or otherwise deterministically deduplicated so multiple fixture instances using the same symbol do not store duplicate bytes.

Only referenced project symbols should be packaged. Project save must not accumulate orphaned symbol resources indefinitely.

### Determinism

Opening a project must not silently regenerate persisted project symbols simply because the current Perastage generator has changed.

A generated project symbol stored in PSTG remains the project representation until one of the following occurs:

- the user explicitly regenerates or replaces it;
- the referenced source definition changes and the user accepts/regenerates the representation according to the product workflow;
- the stored generated resource is missing/corrupt and Perastage must recover through a documented fallback.

User overrides are authoritative project data and must never be discarded as a disposable cache.

## Runtime resolution policy

The existing GUI-independent distinction between standard-GDTF resources and internal rendering remains valuable and should be preserved conceptually.

The physical owner changes:

- `StandardGdtf` resolves standard resources from the GDTF.
- `InternalRendering` resolves Perastage project resources from PSTG/runtime project storage, then uses appropriate standard GDTF or runtime geometry fallback according to the resolver policy.

The resolver must not depend on a private `perastage/symbols/` directory for new content.

Standard and project symbol availability remain independent. A valid standard Top/Side/Front set must not be invalidated because a Perastage Bottom resource is missing or corrupt.

## Legacy compatibility

Perastage must remain able to read historical files produced by older Perastage versions.

Legacy forms include, at minimum:

- `perastage/symbols/...` resources inside GDTF;
- historical Perastage Bottom extensions;
- positively identified historical generated Perastage resources in official SVG locations;
- previous Perastage-specific SVG metadata used to identify owned legacy content.

Legacy compatibility is read-only for these private GDTF extensions.

New writes must not create `perastage/symbols/...` or other Perastage-private GDTF archive resources.

When a legacy Perastage project/GDTF is opened, recognized legacy internal symbols may be used as compatibility input and may be materialized into PSTG project-symbol storage on project save. This migration must not silently claim unknown authored resources as Perastage-owned.

Perastage must not attempt to reconstruct overwritten original authored SVG bytes from a legacy derivative unless an authoritative original source is available. Existing user libraries whose authored SVGs were overwritten by historical Perastage behavior should be rebuilt from clean source GDTFs where practical.

## GDTF modification preference

Perastage should expose a clear project/application policy controlling automatic GDTF completion.

Recommended modes:

### Complete and improve GDTF definitions

Recommended default.

Perastage may:

- add missing standard resources it can generate correctly;
- add missing standard data from explicit/trusted values;
- apply explicit user edits;
- perform safe, deterministic standard repairs covered by Core policy;
- publish a standards-compliant `@Perastage` derivative;
- append standard revisions for effective changes.

It must still not silently replace existing meaningful resources or values merely because a generated alternative exists.

### Preserve imported GDTF definitions

Perastage may inspect and use imported definitions but does not automatically complete or mutate them.

Perastage-specific rendering fallbacks remain PSTG-owned.

Explicit user-requested GDTF edits/repairs may still create a derived GDTF after confirmation unless the product chooses to make this mode a strict lock.

The setting should control GDTF completion/mutation, not normal scene editing inside MVR/PSTG.

## MVR/PSTG relationship

`scene.mvr` inside PSTG remains a standards-oriented scene representation.

Perastage project symbols must not be embedded in the MVR as application-private files or private GDTF resources.

The PSTG container owns any additional Perastage symbol resources outside `scene.mvr`.

When a GDTF has been intentionally completed/edited and the scene references the derived definition, that standards-compliant derived GDTF belongs inside the MVR in the normal way.

When a GDTF was not intentionally modified, MVR export should preserve the original/effective unmodified definition according to the existing canonical MVR policy.

## Cache policy

A future global symbol cache may accelerate generation, but it is not project storage.

Any persistent cross-project cache must:

- be disposable without data loss;
- be versioned;
- use strong source/generator identity in its key;
- have documented size limits and eviction;
- recover safely from corruption;
- never override a persisted PSTG user symbol.

No global name-only or UUID-only symbol library is authoritative.

## Architecture boundaries

Business rules, archive mutation, symbol serialization, project-resource persistence, provenance, and version-aware GDTF output belong in GUI-independent Core services.

GUI code may:

- present previews and diagnostics;
- request completion/replacement/regeneration;
- collect explicit user intent;
- select policy options.

GUI code must not hand-roll GDTF archive rewriting, revision XML, PSTG symbol packaging, or resource-resolution rules.

Existing reusable infrastructure should be preserved where its responsibility remains valid, including:

- fixture symbol inspection/resolution;
- vectorization and polygon-with-holes support;
- SVG parsing and rendering;
- semantic fingerprinting;
- GDTF canonicalization;
- GDTF mutation audit/revision helpers;
- atomic derivative publication;
- PSTG transactional additional-resource packaging;
- existing cache invalidation boundaries.

## Required migration from the previous implementation

The previous implementation wrote new internal Perastage symbols under `perastage/symbols/` inside derived GDTFs. That is no longer an allowed write target.

Implementation must:

1. stop all new writes to `perastage/symbols/`;
2. keep safe legacy reads;
3. move current/future Perastage project-symbol persistence to PSTG;
4. keep standard GDTF resources independent from project symbols;
5. generate missing standard GDTF SVGs through a dedicated standards-compliant serializer/output path;
6. never replace an existing valid authored standard SVG automatically;
7. make explicit replacement/repair a distinct user-intended operation;
8. remove Perastage-private SVG markers from newly generated standard GDTF SVG resources;
9. keep Perastage-specific symbol metadata only in PSTG-owned resources;
10. update the existing fixture symbol resource contract and GDTF mutation policy so documentation matches this contract.

## Acceptance criteria

The architecture is accepted only if all of the following are true.

### Standard preservation

- Opening/inspecting a clean GDTF does not mutate its source bytes.
- An existing valid authored standard SVG is not automatically replaced.
- A missing GDTF 1.2 Top/Side/Front resource can be completed in a derived GDTF when policy permits.
- Generated standard SVG names/paths match `Model/@File` and official GDTF locations exactly.
- Standard model offsets are written through official GDTF attributes.
- Newly generated standard SVGs contain no Perastage-private ownership metadata.
- Effective GDTF mutations append one appropriate standard revision.
- No-op repeated publication adds no revision.
- Published GDTFs pass the shared canonical publication/validation boundary.

### Project-symbol persistence

- A generated Perastage project symbol survives PSTG save/load without regeneration.
- A user override survives PSTG save/load and is never treated as a cache entry.
- Multiple fixtures using identical symbol content do not require duplicate packaged payloads.
- Project save packages only referenced symbol resources.
- Project symbols are not added to `scene.mvr` or embedded GDTFs.
- Deleting a disposable external/global cache cannot change a correctly saved project's representation.

### Legacy compatibility

- Legacy GDTFs containing `perastage/symbols/` remain readable.
- Legacy Perastage-owned symbols can be migrated into project-owned PSTG resources without rewriting the external source merely because it was opened.
- Unknown authored SVGs are never classified as Perastage-owned without positive evidence.
- New publication never creates the legacy private namespace.

### Mutation policy

- Automatic completion adds missing standard content only when enabled.
- Existing valid content requires explicit intent to replace.
- Invalid existing content is reported separately from missing content and requires explicit repair/replacement.
- User-entered physical/data edits can produce audited standards-compliant derived GDTFs.

## Non-goals for the initial migration

The initial migration does not need to implement:

- a global permanent symbol library;
- cloud synchronization of project symbols;
- automatic recovery of original SVGs overwritten by historical Perastage versions;
- automatic adoption of future GDTF `next` features while writing GDTF 1.2;
- a full GDTF editor for every standard field;
- unrelated MVR/CLI/OSC roadmap work.

The goal is to establish correct ownership, standards-compliant GDTF completion, safe mutation semantics, and deterministic project persistence before continuing broader Perastage 1.7.0 work.
