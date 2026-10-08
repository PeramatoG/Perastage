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

## Inspection, application, and future standard filling

Availability and derivative publication accept a complete standard set or a
complete internal set independently. Missing or malformed internal Bottom
cannot invalidate an otherwise usable standard Top/Side/Front set. Runtime
loading prefers a usable dedicated symbol, then a compatible legacy or standard
resource, and retains the existing requested-view-to-Top fallback. Loaded data
records the actual resource set and provenance plus whether a different view
was used; a fallback does not make a missing stored view exist.

Current symbol application writes internal resources only. Existing authored
standard SVG bytes and offsets are preserved, including unusable authored
resources. Missing standard views remain missing even when an internal symbol
for the same logical view is available. Publication operates on a private
working derivative and leaves the source GDTF immutable.

The model supports a future workflow that detects a missing standard view,
derives it from an available internal symbol, converts it to the exact standard
GDTF representation, and adds it to the derivative. Explicit generated standard
output can be identified with `data-perastage-resource-set="standard-gdtf"`
alongside the version marker, in the official location. Its provenance remains
`GeneratedPerastage`, but its set is `StandardGdtf`. This PR implements the
inspection contract for that distinction, not conversion, automatic filling,
or authored-resource replacement. Replacing an authored standard view must be
an explicit user action in any later workflow.

Consolidation excludes only resources in the Perastage set and preserves
standard SVG contents and offsets, including explicitly generated standard
resources. The semantic fingerprint includes both official SVG locations and
the dedicated namespace, so changes to symbols, offsets, or provenance markers
affect generation identity without a persistent symbol manifest.
