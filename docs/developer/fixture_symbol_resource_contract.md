# Fixture symbol resource contract

`core/symbols/fixture_symbol_resource_contract.h` owns the GUI-independent
resource model. Inspection and loading reuse the shared GDTF archive reader
and the existing SVG parser. Both leave the original archive unchanged.

## Independent resource sets

Each logical view can have both a standard GDTF resource and a separate
Perastage-owned resource. Resource-set membership is independent of provenance:
a standard resource can be authored or generated, while an internal Perastage
symbol always remains in the Perastage set.

| Set | Archive locations | Completeness |
| --- | --- | --- |
| Standard GDTF | Top: `models/svg/<model>.svg`; Side: `models/svg_side/<model>.svg`; Front: `models/svg_front/<model>.svg` | `standardViewsUsable` covers only these three standard views. There is no standard Bottom. |
| Internal Perastage | `perastage/symbols/<model>/top.svg`, `side.svg`, `front.svg`, `bottom.svg` | `perastageViewsUsable` retains the existing internal four-view representation, including Bottom. |

The existing renderer represents GDTF Side using `SymbolViewKind::Left` and
`Right`. Each resource records its view, archive path, existence, usability,
provenance, set membership, and diagnostics. Completeness here describes
renderer-usable symbols, not schema validity of the entire GDTF package.
Partial resources remain individually available to inspection and loading.

`standardViews` and `perastageViews` expose both sets independently.
`perastageResources` additionally retains all known internal alternatives,
including legacy entries that coexist with newer dedicated symbols. This
prevents consolidation from forgetting positively owned legacy resources
when a dedicated symbol takes runtime precedence.

## Provenance and compatibility

| Provenance | Evidence |
| --- | --- |
| `AuthoredGdtf` | A standard resource without positive Perastage-generation evidence. Conventional model names such as `base` and `main` have no ownership meaning. |
| `GeneratedPerastage` | A resource in the dedicated internal namespace, or an explicitly identified generated standard resource. New internal SVGs carry `data-perastage-symbol-version="1"`. |
| `LegacyPerastage` | An old Bottom extension, a version-marked internal SVG in an official location without an explicit standard declaration, or a standard-location view identified by an exact historical fixture-symbol revision listing that view. These compatibility resources belong to the Perastage set. |
| `RuntimeFallback` | Reserved for generated runtime representations without a stored archive resource. |
| `None` | No resource exists in that set. |

Generic Editor values, unrelated revisions, and old mutation-audit nodes do
not establish standard-resource ownership. The historical revision action
`Applied fixture SVG symbol views (...)` identifies only the listed legacy
views. The new action `Applied Perastage fixture SVG symbol views (...)`
identifies internal generation and does not claim existing standard resources.

Legacy `models/svg/<model>_bottom.svg` and
`models/svg_bottom/<model>.svg` remain readable as internal extensions.
Recognized legacy symbols in official locations remain on disk unchanged;
inspection does not present them as standard resources. New writes always
use the dedicated namespace.

Internal offsets are stored on each generated SVG root using
`data-perastage-offset-x-mm` and `data-perastage-offset-y-mm`. They do not alter
the standard model SVG offsets in `description.xml`. Legacy resources retain
read compatibility with the historical model-offset attributes. Unknown
resource-marker versions remain explicitly identifiable, with a diagnostic
when their existing SVG geometry can still be used.

## Internal SVG vector serialization

`core/symbols/Symbol2DSvg.{h,cpp}` owns pure fixture-level SVG serialization.
The GUI exporter delegates to it and retains only its file-output wrapper.
Each `PolygonWithHoles2D` with drawable holes becomes one SVG 1.1 compound
`path` with `fill-rule="evenodd"`: the outer ring is first, followed by each
hole, using absolute `M`/`L` commands and an explicit `Z` for every ring.
Hole-free regions retain their existing `polygon` representation. Gray fills,
independent black polylines, stroke width, coordinate conversion, numeric
formatting, bounds, and the four-view capture/vectorization pipeline are unchanged.

The reader accepts this deterministic path subset for positively identified
Perastage resources and reconstructs the outer and hole contours without
geometric simplification or winding changes. Authored SVG recovery retains its
existing scope. The reader also
retains historical white-filled polygon recovery. New holes expose the actual
background instead of painting white, while preserving appearance on white.
Version/provenance markers, internal offsets, and archive paths remain unchanged.
Publishing the new representation over an old generated white-hole resource is
one effective payload change and creates one revision; applying it again changes
neither resource bytes nor revision count. Reading alone never migrates resources.

## Inspection, application, and future standard filling

Availability and derivative publication accept a complete standard set or a
complete internal set independently. Missing or malformed internal Bottom
cannot invalidate an otherwise usable standard Top/Side/Front set. Per-view
selection is owned by the GUI-independent `fixture_symbol_resolution` service,
which consumes `FixtureSymbolResourceInspection` without parsing archives.
`FixtureSymbolResolutionPurpose` makes caller intent explicit:

- `StandardGdtf`: usable authored standard, explicitly generated standard,
  recognized legacy Perastage for the same view, then runtime/geometry fallback.
  Dedicated internal content does not fill missing standard Top/Side/Front.
- `InternalRendering`: dedicated Perastage, recognized legacy Perastage,
  standard content for the same view, then the existing Top-view fallback or
  runtime/geometry fallback.
- Bottom has no standard candidate. Both purposes consider dedicated and legacy
  Perastage Bottom; only internal rendering permits the existing Top-view fallback.
- Right resolves the Side (`Left`) resource while loaded rendering data retains
  Right orientation. Side compatibility does not count as a different-view fallback.
  Internal Back requests retain Top fallback.

Each result records requested and actual view, set, provenance, exact archive
path, existence, usability, offsets, view-fallback use, a typed fallback reason,
and diagnostics (including rejected malformed candidates). Runtime/geometry
results have no stored path, `exists=false`, `usable=false`, and
`RuntimeFallback` provenance; rendering remains the consumer's responsibility.
`fixture_symbol_availability` exposes `standardViews` and `internalViews` for
all six `SymbolViewKind` values in enum order. Its aggregate completeness flags
retain their prior independent-set meaning; they are not computed from fallback
results. Partial resources are available through individual resolutions.

The SVG loader follows the resolver's exact path and metadata. The managed SVG
cache includes purpose in its key and owns caching only. Consumers continue
through availability/loading; no GUI or Viewer candidate policy is introduced.

Current symbol application writes internal resources only. Existing authored
standard SVG bytes and offsets are preserved, including unusable authored
resources. Missing standard views remain missing even when an internal symbol
for the same logical view is available. Publication operates on a private
working derivative and leaves the source GDTF immutable.

For an unsaved scene, an absolute resolved source does not require a project
folder. Application prepares a private working copy in active fixture-library
ownership, publishes it canonically and atomically, and retargets matching
in-memory references to its absolute owned path. It never assigns a synthetic
`basePath`. `ApplySymbolsResult::fixtureReferencesUpdated` records this in-memory
outcome separately from `sceneUpdated`, which means project archive persistence.
A successful unsaved application reports library-backed success and reminds the
user to save the project. Both symbol and semantic-revision caches are updated.

The comparison preview model consumes inspection and the existing resolver,
but accepts only a usable stored resource in the requested set and logical view.
Rendering fallback to another set or Top is not an original-resource preview.
Standard Bottom is always unavailable, even if internal Bottom is usable.
Missing and malformed resources retain their own path/provenance/diagnostic
for the editor's independent Standard GDTF and Perastage groups.

Publication compares complete generated SVG payloads (including internal offset
and version metadata) with the existing dedicated entries. It replaces only
changed payloads and appends a symbol revision listing only changed views. The
shared audit helper's explicit `RecordEffectiveChange` policy records subsequent
real changes to the same views; other audit callers retain action deduplication. If
all supplied payloads match, it leaves the working archive and description
unchanged; normal validation and canonical publication still run. Reapplying
identical symbols preserves resource inspection, description metadata and
semantic fingerprint. Geometry or offset changes still create a revision and
change derivative identity. Authored standard payloads and model offsets are
never rewritten by the symbol operation.


The model supports a future workflow that detects a missing standard view,
derives it from an available internal symbol, converts it to the exact standard
GDTF representation, and adds it to the derivative. Explicit generated standard
output can be identified with `data-perastage-resource-set="standard-gdtf"`
alongside the version marker, in the official location. Its provenance remains
`GeneratedPerastage`, but its set is `StandardGdtf`. Inspection and resolution
support that distinction without conversion, automatic filling, or
authored-resource replacement. Replacing an authored standard view must be
an explicit user action in any later workflow.

Consolidation excludes only resources in the Perastage set and preserves
standard SVG contents and offsets, including explicitly generated standard
resources. The semantic fingerprint includes both official SVG locations and
the dedicated namespace, so changes to symbols, offsets, or provenance markers
affect generation identity without a persistent symbol manifest.
