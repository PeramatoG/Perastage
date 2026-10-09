# Fixture symbol generation and runtime cache

## Resource and publication contract

The [Core resource contract](fixture_symbol_resource_contract.md) and
[GDTF symbol ownership and mutation contract](gdtf_symbol_ownership_and_mutation_contract.md)
separate two output products. Standard GDTF Top/Side/Front use the exact
`Model/@File` in official model SVG locations. Perastage Top/Front/Side/Bottom
symbols are project-owned and persisted in PSTG `resources/fixture_symbols/`
outside `scene.mvr`, with exact bytes, independent offsets, provenance, and
explicit fixture bindings. Historical private GDTF symbols are read-only
compatibility inputs; new writes never create `perastage/symbols/`.

Manual Apply stores a user override in project/session state, including unsaved
scenes without a project base path. Automatic preparation stores generated
fallback bundles when no authoritative internal representation is available.
When trusted source geometry is unavailable, Core can persist the current
fixture-cube convention as a project placeholder; it never completes standard
GDTF resources or invents physical metadata. Persisted symbols take precedence;
a generator change or empty disposable cache does not regenerate them. Core owns packaging through transactional additional
resources. Save includes only referenced bundles and Load restores exact content;
neither operation waits for the preparation queue or captures new geometry.

Standard completion is a separate Core operation. In the default completion
policy it adds missing GDTF 1.2 Top/Side/Front resources from captured geometry,
using the dedicated standard serializer and official model offsets. Existing
usable and unusable authored resources are preserved automatically. Preserve
mode disables this GDTF mutation while allowing project fallbacks. Effective
completion receives one standard Revision and a canonical atomic `@Perastage`
derivative; a no-op adds none. Fixture references change only after successful
publication. Standard and internal completeness are independent; Bottom is
never a standard completeness requirement.

`fixture_symbol_availability.*` is the GUI-independent loading boundary for
Viewer2D/Viewer3D, layout, legends, PDF, comparison previews, and preparation.
Project lookup precedes existing GDTF candidate policy for internal rendering.
The shared Core resolver owns standard/legacy fallback order and records the
selected resource's set and provenance independently from view fallback.
Inspection never counts fallback as an exact stored resource.

## Runtime preparation

The MainWindow-owned runtime coordinator keys work by canonical physical GDTF
resource and exact mode. Duplicate fixtures and renderer fallback requests
produce one logical job. Each fixture uses one non-yielding GUI-thread capture
for warm-up plus all four views; pure image/vector processing runs on the
managed GUI/OpenGL-free worker. The offscreen renderer is rebound to the active
project before control returns to layout preview or print capture.

The private capture compatibility boundary temporarily swaps only live
renderable containers so modern and legacy renderer queries see the same
single target continuously. Strict RAII restores fixtures, trusses, scene
objects, and supports once on every exit; no event processing occurs inside the
capture. It remains a narrow capture adapter, not general scene architecture.

Project epochs reject work captured for a replaced or closed project. Jobs
recheck the strong symbol-relevant semantic fingerprint before application to
reject in-place source changes. This low-frequency correctness check remains
separate from hot rendering cache identity. Preparation does not persist queue,
snapshot, or worker state. Persisted bundle metadata belongs to the project,
not the coordinator.

Recognized legacy internal views are materialized into project storage before
automatic standard completion retargets a GDTF reference. Newly inserted
instances inherit an existing project symbol only when every bound instance of
that exact nonempty source/mode agrees on the same content. Divergent overrides
remain authoritative and are diagnosed rather than merged or regenerated.

Manual preview pauses matching automatic work. Applying records an explicit
project override; closing without Apply restores automatic eligibility. A
matching authoritative persisted override prevents automatic replacement.
Core owns project application and standard completion rules; GUI owns capture,
progress, diagnostics, and refresh.

## Hot SVG cache

`FixtureSymbolSvgCache` stores immutable positive GDTF parse results. Its key
uses canonical physical identity, requested `SymbolViewKind`, explicit
resolution purpose, bounded file revision (size and modification time), and
SVG parser/schema version. Repeated lookups perform filesystem metadata reads
without hashing or reading whole archives merely for cache identity. Failed
loads are not cached, so newly available views remain discoverable.

Project bundle identity is deterministic and content-addressed; project symbols
are not cache entries. Successful standard derivative replacement invalidates
the physical GDTF path before refresh. New/Close/reset and project replacement
clear runtime parse state while authoritative project resources follow the
project lifecycle and serialization. Immutable handles remain safe after cache
invalidation. No global symbol cache or permanent library owns a project's
representation.

## Layout and background export validation

Live layout frame identity includes the immutable project-symbol source token,
so an override changes presentation even when its GDTF reference is unchanged.
Apply and Undo/Redo notify visual-content invalidation before the layout is
rendered again. Captured paths and explicit project tokens remain authoritative;
legacy name-only lookup cannot select another fixture's project override.

The existing `project_cache::ValidationContext.packagedLayoutResourceFingerprint`
uses `fnv1a64-v1` over sorted named payload fingerprints. It now covers layout
images and referenced PSTG fixture-symbol `index.json`, bundle manifests, and
available SVGs. Save enumerates referenced symbol resources; Load fingerprints
original ZIP bytes after validating symbols and restoring fixture bindings.
Orphan bundle files and disposable `layout_view_cache` files are excluded. This
adds no cache schema field/version and prevents a changed offset or override
from accepting an old rendering merely because `scene.mvr` is unchanged.

PDF export captures a `shared_ptr<const ProjectFixtureSymbolStore>` before
starting background work. `ScopedProjectFixtureSymbolReadContext` pins that
snapshot for all token generation, inspection, and loading on the worker thread.
Its cache checks the scoped content identity before returning parsed data;
active-project replacement or cache clearing cannot substitute another symbol.
Nested scopes restore the previous context, and an explicit null scope exposes
no symbols rather than falling through to the live project provider.
