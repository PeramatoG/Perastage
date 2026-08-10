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
therefore produce one logical job. Each idle activation performs at most one
bounded capture slice, every slice reads one immutable scene snapshot, and pure
image/vector processing runs on the managed GUI/OpenGL-free worker.
After each isolated slice, the shared offscreen 2D renderer is rebound to the
active project scene before control returns to layout preview or print capture.
The scoped boundary pairs every scene-replacement preparation with completion,
including render failures, and synchronizes the active project only after the
capture snapshot has been released. This restoration is mandatory even when
more automatic views remain queued.

Each isolated capture slice also performs one bounded warm-up through the normal
offscreen render lifecycle before `FitViewToScene()`. `UpdateScene(true)` alone
is not a sufficient pre-fit boundary because resource and world-bounds
synchronization normally occurs later in the render path. The warm-up uses the
snapshot's resource base and ignores live-project visibility filters, so every
renderer subsystem observes the same isolated scene. This prevents an
interleaved project or layout render from supplying stale bounds to Front, Top,
Side, or Bottom fitting.

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
