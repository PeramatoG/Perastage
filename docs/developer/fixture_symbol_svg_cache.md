# Fixture symbol generation and runtime cache

## Resource and publication contract

An external or source GDTF remains authoritative when its exact top, bottom,
front, and side SVG model views are parseable, have a positive view box, and
contain usable geometry. Availability is ownership-neutral: standard GDTF
content is usable without Perastage Editor or revision metadata.

When views are missing or invalid, renderers use their existing geometry
fallback immediately. Runtime preparation copies the source to a temporary
working derivative, generates all four views, validates the complete archive,
and atomically publishes the canonical `@Perastage` derivative. Fixture
references are rebound only after publication succeeds, so a failed operation
leaves the previous file and reference authoritative.

`fixture_symbol_availability.*` is the GUI-independent inspection and loading
boundary. Viewer2D/Viewer3D rendering, layout rendering and legends, PDF export,
automatic preparation inspection, and manual apply validation use the same
stored-SVG rules. A fallback is never reported as a stored SVG.

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
3. bounded file revision (file size and modification time); and
4. SVG parser/schema version.

Normal repeated lookups perform filesystem metadata reads but never hash or read
the complete archive merely to establish cache identity. Failed loads are not
cached, so fallback remains responsive to a newly published view.

Successful derivative replacement explicitly invalidates the physical path
before refresh. Manual Apply uses the same publication boundary. Successful
project replacement, New, Close, and reset clear project/session runtime symbol
state. Existing immutable handles remain safe after invalidation or clear, and
the next consumer lookup receives the replacement without reopening the project.
