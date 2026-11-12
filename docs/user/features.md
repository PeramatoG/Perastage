# Feature Overview

Perastage is an open-source desktop tool for lighting and rigging scene work and technical documentation.

## MVR project workflows

[Open, merge and export MVR](opening-mvr-files.md), review conflicts and save ongoing work as PSTG projects.

## GDTF and GDTF Share

Use fixture definitions and local dictionaries, [search/download GDTFs](gdtf-download.md), browse cached catalog data offline and review revisions and DMX modes. The Fixture Editor inspects Models and preserves authored GDTF resources.

## MVR / GDTF Inspector

[Inspect](inspector.md) project snapshots and external packages without editing them. Browse scene/package trees, fixture definitions, XML, previews and reported issues.

## 2D and 3D visualization

[Review plans and spatial relationships](views.md), choose rendering styles and use movement, axis and snapping controls. Adjust model detail for larger scenes.

## Scene editing, tables, patch and grouping

Edit fixtures, trusses, hoists and objects; review metadata and patch; organize layers and groups. Use table edits, continuous placement, Undo/Redo and the embedded [Console](shortcuts-and-command-bar.md).

## Create from Text

[Generate starting scenes](create-from-text.md) from pasted text, TXT or PDF riders, review fixture-type matches, choose modes or continue with Generic fallback.

## Layouts and PDF documentation

[Arrange printable pages](layouts.md) with 2D views, legends, event tables, text and images. Share templates and export layout PDFs.

## Experimental local integration

OSC and [MCP](../developer/mcp_adapter.md) offer experimental local integration for separately configured automation. Technical contracts belong in the [developer documentation](../developer/index.md).

## Platforms and source

Perastage supports Windows, macOS and Linux and is licensed under GNU GPL version 3 or later. See [Installation](installation.md) for release packages and the [repository](https://github.com/PeramatoG/Perastage) for source and contribution information.

## Manual DMX patching

Manual patching of one fixture preserves a valid Universe/Channel start (channel 1–512), even when its primary GDTF footprint exceeds that universe. A red Channel cell explains overflow, unresolved footprints or overlaps on hover. For example, 180 channels at 1.400 would span 1.400–1.512 and 2.1–2.67 under hypothetical continuous Auto Span routing. This is informational: standard MVR stores the start, does not guarantee that routing, and the receiving/controller application must support it. Secondary DMX breaks stay unchanged. AutoPatch and intentional multi-fixture sequential patching retain their existing universe-wrapping behavior.

## DMX Patch viewer

Open **View → DMX Patch** to inspect the 512 channels of any universe below the active 2D/3D viewport. The compact cells and column count adapt automatically to the available width; enter a universe number to inspect an empty universe. Colors follow fixture presentation colors, labels show physical GDTF channel attributes, and hover text identifies every occupant and break. Striped cells indicate overlaps. The diagnostic list explains missing definitions, assignments and overflow; channels beyond 512 are not continued into another universe. This viewer is read-only. Close it to reclaim viewport space; visibility and height are remembered. It is temporarily hidden in Layout and Inspector modes. Use the caption controls to close or maximize the panel. The warning icon opens a scrollable diagnostic list instead of a long tooltip.

Each fixture name appears only on its first row; centered channel numbers, a boxed first channel and rounded start/end corners identify its extent. For ambiguous definitions, enable **Declared offsets** to inspect explicitly provisional bytes relative to the primary start: dashed borders and question marks distinguish them, and the provisional count is separate from confirmed occupancy and conflicts. This option does not repair breaks, rewrite files or span into another universe.
