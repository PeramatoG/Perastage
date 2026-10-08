# Fixture symbol generation and runtime cache

## Resource and publication contract

The [Core resource contract](fixture_symbol_resource_contract.md) describes
each stored SVG view, its provenance, and its resource set. Standard GDTF Top,
Side, and Front remain in their official model SVG locations. Internal
Perastage symbols use a separate `perastage/symbols/<model>/` namespace and
retain their four-view representation, including Bottom. Both resources can
coexist for the same logical view. Bottom is never required for standard-view
completeness. Authored resources named `base` or `main` remain authored.

When neither set is complete, renderers retain their existing per-view and
geometry fallbacks. Runtime preparation copies the source to a temporary
working derivative and captures all four internal views. Application writes
only the dedicated internal namespace, including independent SVG-root offsets;
it preserves existing standard SVG bytes and model offsets. Missing standard
views stay missing: standard conversion and replacement are future workflows.
A complete standard set or complete internal set can be atomically published as
a canonical `@Perastage` derivative. Fixture references are rebound only after
publication succeeds, leaving the source and previous reference unchanged on
failure.

`fixture_symbol_availability.*` is the GUI-independent inspection and loading
boundary. Viewer2D/Viewer3D rendering, layout rendering and legends, PDF export,
automatic preparation inspection, and manual apply validation use this model.
The shared Core resolver owns candidate order for an explicit standard or
internal purpose. Internal runtime loading prefers usable dedicated resources
and retains the existing Top fallback. It records the selected resource's set and provenance separately
from view fallback; inspection never counts fallback as an available stored SVG.

## Runtime preparation

The MainWindow-owned runtime coordinator keys work by canonical physical GDTF
resource and exact mode. Duplicate fixtures and renderer fallback requests
therefore produce one logical job. Each fixture uses one non-yielding GUI-thread
capture operation for warm-up plus all four views, and pure image/vector
processing runs later on the managed GUI/OpenGL-free worker.
After the complete capture, the shared offscreen 2D renderer is rebound to the
active project scene before control returns to layout preview or print capture.
The scoped boundary pairs every scene-replacement preparation with completion,
including render failures, and synchronizes the active project only after the
temporary capture scene has been restored.

The private capture compatibility boundary temporarily swaps only the live
renderable containers, exactly as the established synchronous generator did.
This is deliberately not a general scene architecture: it guarantees that both
modern scene accessors and legacy direct `ConfigManager` queries observe the
same single target continuously from warm-up through Front, Top, Side, and
Bottom. Strict RAII restores the project once on every exit path, and no event
processing occurs while the compatibility boundary is active.

Project epochs reject work captured for a replaced or closed project. Automatic
jobs also compute the strong symbol-relevant semantic fingerprint at job start
and immediately before publication. That low-frequency correctness check rejects
in-place source changes; it is deliberately separate from hot rendering cache
identity.

Save, Load, MVR import/export, layout, print, and PDF never inspect queue
completion and never wait for preparation. No preparation queue, snapshot, or
symbol manifest is persisted. Reopening a project simply resolves the published
GDTF and recognizes valid stored SVGs. Manual preview pauses matching automatic
work; Apply intentionally publishes through the same transaction, while closing
without Apply restores automatic eligibility.

## Hot SVG cache

`FixtureSymbolSvgCache` stores immutable positive parse results. Its key consists
only of:

1. canonical physical filesystem identity;
2. requested `SymbolViewKind`;
3. explicit resolution purpose (standard GDTF or internal rendering);
4. bounded file revision (file size and modification time); and
5. SVG parser/schema version.

Normal repeated lookups perform filesystem metadata reads but never hash or read
the complete archive merely to establish cache identity. Failed loads are not
cached, so fallback remains responsive to a newly published view.

Successful derivative replacement explicitly invalidates the physical path
before refresh. Manual Apply uses the same publication boundary. Successful
project replacement, New, Close, and reset clear project/session runtime symbol
state. Existing immutable handles remain safe after invalidation or clear, and
the next consumer lookup receives the replacement without reopening the project.
