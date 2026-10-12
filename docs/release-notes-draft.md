# Perastage 1.7.0

Perastage 1.7.0 focuses on understanding MVR/GDTF files, preserving fixture
definitions and project symbols, and making everyday viewing and editing more
comfortable. It also introduces experimental command-line and local automation
workflows.

## Highlights

- New read-only **MVR / GDTF Inspector** for projects, external files and embedded
  fixture definitions.
- Self-contained **PSTG fixture symbols** and per-Model inspection of authored
  standard GDTF views.
- Faster **GDTF Share browsing**, with richer search, revision and DMX mode details.
- **Low / Standard / High 3D Model Detail** and reorganized Preferences.
- An experimental **CLI included in release packages**, plus local OSC and a
  separately configured MCP integration path.
- Unified **offline Help** in English, Spanish and Simplified Chinese.

## MVR / GDTF Inspector

- Inspect a snapshot of the current project, refresh it after editing, or open
  external MVR/GDTF files without importing or changing them. Browse scene
  hierarchy, package contents and embedded GDTFs, then return to the parent MVR.
- Review fixture metadata, DMX modes, channels, functions, wheels and resource
  references. Examine original XML with search and folding, and review issues
  and compatibility diagnostics offline.
- Preview SVGs, images and supported 3D models beside their source. Resizable
  panes and background loading keep larger packages responsive; oversized or
  unsupported resources remain listed with a clear preview status.

## GDTF and fixture symbols

- Perastage symbols and manual overrides now travel with the **PSTG project**.
  Apply them even before the first save, then save the project to retain them.
  These project symbols are not exported into MVR or embedded GDTFs.
- Manufacturer-authored GDTF **Top / Side / Front** resources are preserved.
  The Fixture Editor selects each Model independently and keeps project symbols
  in their own fixture-wide group.
- Automatic fixture-symbol preparation writes Top/Front/Side/Bottom to PSTG
  while preserving source GDTF bytes, Model offsets and fixture references.
- The Preview page offers **All fixture** or **Solo** inspection of Geometry
  subtrees and resolves canonical root SVG/PNG thumbnails.
- Standard SVG resource presence is separate from Perastage preview support;
  manufacturer resources remain visible and preserved even when unsupported.
- Existing unusable or authored resources are not silently replaced; unusable
  views require explicit repair. **Bottom** remains a Perastage project view,
  rather than a standard GDTF 1.2 SVG requirement.
- Symbol transparency and holes now reveal the actual background correctly.
  Supported legacy private Perastage symbols remain readable and can migrate
  into PSTG; new GDTF output does not embed private Perastage symbols.

## GDTF Share

- A usable cached catalog opens immediately, including offline browsing.
  Background refresh keeps browsing available, and a failed refresh retains the
  cached results. Online downloads still require authentication.
- Search words can match in any order across manufacturer, fixture, UUID,
  revision and DMX mode. A resizable results list shows revision metadata and
  mode channel footprints before downloading.

## Workflow and usability

- Choose **Low**, **Standard** (default) or **High** 3D Model Detail to balance
  rendering detail and performance. Changes take effect on redraw without
  altering source models, 2D symbols or exported geometry.
- Enable the optional **simplified proxy while navigating** to temporarily
  simplify unselected fixtures during fast navigation, then restore the chosen
  detail.
- Preferences now use categories for General, Import, Viewer and Formats.
- **General → Cache** shows cache storage and disk usage, opens the cache folder,
  and clears regenerable caches immediately after confirmation. Geometry caches
  now stay in Perastage user data instead of beside GDTF and model files; existing
  catalog caches migrate safely and settings, libraries and projects are preserved.
  Switching categories retains pending edits; **Apply** saves without closing.
- Tables use numeric and natural ordering, object dragging follows the pointer
  smoothly in 2D/3D, and Layout Viewer view dragging/resizing stays on the
  selected view when legends refresh.

## Experimental automation and CLI

- Perastage now ships an experimental **perastage-cli** for advanced automation
  and integration workflows, independent of GUI startup. **inspect** reads
  MVR/GDTF files without changes; **scene** processes an explicit input MVR into
  a separate output, preserving the source and publishing no partial result on
  failure. Existing outputs require explicit overwrite permission. **live**
  connects to a running instance on the local computer;
  **capabilities** discovers supported machine operations.
- The built-in experimental **OSC** interface is local-only and bounded to
  selection clearing and explicit position/rotation changes. Live transforms
  preserve selection and use normal validation and Undo; targeted live batches
  apply together or leave the scene unchanged.
- An experimental **MCP integration path** uses a separately configured Rust
  adapter that is **not included in normal Perastage packages**. It provides
  local inspection and supported live operations, not remote or general network
  control. Perastage has **no built-in AI**.

See the [CLI guide](developer/cli.md), [local OSC interface](developer/command_api.md#osc-adapter)
and [MCP adapter guide](developer/mcp_adapter.md) for setup and supported operations.

## Compatibility, stability and important fixes

- Restored broader **Linux AppImage compatibility**, targeting Ubuntu 22.04
  with glibc 2.35 or newer.
- Fixed Inspector crashes and navigation problems when opening embedded GDTFs
  on Windows, improved cross-platform preview reliability, and corrected
  persisted Inspector pane settings on macOS 15.
- Improved MVR import and inspection fidelity for hierarchy, layers, Symdefs,
  positions, fixture categories, GDTF mode resolution, embedded resources and
  Unicode paths. GDTF publication preserves external sources and unknown vendor
  metadata while cleaning known legacy Perastage markers in published copies.

- Grouping, ungrouping and supported fixture/object conversions use one Undo
  step and avoid empty history entries when nothing changes. Scene-object to
  truss conversion no longer risks replacing an existing truss with the same
  identifier.

## Documentation and Help

- One canonical user manual now supplies repository documentation, the website
  and packaged offline Help in **English, Spanish and Simplified Chinese**.
- Console help comes from the same documentation source.

## Downloads

Choose the corresponding asset on the **1.7.0 GitHub Release**:

| Platform | Package |
|---|---|
| Windows 64-bit | Installer |
| Linux x86-64 | AppImage |
| macOS 15 Apple Silicon | DMG |
| Current macOS Apple Silicon | DMG |
| Arch Linux x86-64 | Package |

The actual GitHub Release assets are authoritative. The experimental CLI is
included with the corresponding normal Perastage package; no separate CLI
download is required.

Documentation:
https://perastage.luismaperamato.com/documentation.html

Issues / feedback:
https://github.com/PeramatoG/Perastage/issues
