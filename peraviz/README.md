# Peraviz – real-time DMX 3D viewer based on MVR/GDTF

## Overview

**Peraviz** is a project developed in parallel to **Perastage** with the goal of becoming a **real-time 3D DMX visualizer**.  
While Perastage focuses on project management and editing workflows, Peraviz is intentionally scoped as a **viewer-first** tool: load an **MVR** scene, interpret **GDTF** fixtures as reliably as possible, and render the result while reacting to **live DMX**.

Peraviz is designed to work independently, but it may later become a partially integrated module inside Perastage (for example, to reuse project assets and scene organization) while still remaining usable as a standalone viewer.

### Key principles

- **MVR/GDTF as source of truth**  
  Peraviz loads `.mvr` scenes and reads `.gdtf` fixture definitions to drive both geometry placement and DMX behavior.

- **Visualization, not editing**  
  The current focus is rendering and validation. Any editing is expected to remain minimal (if added at all), and Perastage remains the primary tool for authoring.

- **Correct transforms and predictable units**  
  Import correctness is treated as a first-class concern. Peraviz includes debug modes and a baseline workflow to validate axis conventions, handedness, and scale.

- **Godot 4 + native loader**  
  Runtime/UI logic is implemented in Godot scripts, while MVR/GDTF parsing and heavy lifting is performed in a C++ GDExtension.

## Current status (February 2026)

This section summarizes what is implemented today.

### 1) MVR loading and scene reconstruction

- **Native loader (GDExtension): `PeravizLoader`**
  - Loads an `.mvr` file and returns a list of render nodes (id, parent id, name/type/class, local transform, asset references, and fixture metadata).
  - Can load **3DS mesh data** for fixtures/assets.
  - Exposes fixture patch info and can build DMX control bindings (when DMX support is enabled).

- **Reuse of Perastage building blocks**
  - Peraviz currently reuses foundational Perastage utilities for transform parsing/composition and MVR schema compatibility.
  - The full geometry/material pipeline is not reused yet; Peraviz relies on runtime proxies and selective asset loading.

### 2) Runtime viewer and UI

- **Main viewer script (`scripts/load_scene.gd`)**
  - Provides a file picker to load `.mvr` scenes.
  - Builds a node tree in Godot, instantiating proxies and/or loading real assets when available.
  - Includes tools for camera focus, selection, and debugging overlays.

- **Fixture selection + manual testing**
  - A manual test panel allows selecting a fixture and applying pan/tilt/dimmer controls for validation (useful even without live DMX).

- **Visual Settings**
  - A dedicated window exposes multipliers and quality settings to tune the look and performance.

### 3) Live DMX (Art-Net) support

- **Art-Net receiver (native)**
  - Peraviz includes a receiver class that can listen for Art-Net packets (default port `6454`) and expose per-universe frames to scripts.
  - UI elements allow toggling DMX on/off, monitoring activity, and applying a universe offset.

- **Fixture control runtime (`scripts/dmx_fixture_runtime.gd`)**
  - Builds per-fixture DMX bindings from the parsed scene and GDTF data.
  - Reads **8-bit**, **16-bit**, and **24-bit** control values (coarse/fine/ultra-fine).
  - Applies controls at runtime via a callback so the viewer can update transforms and visual output.
  - Reports unbound fixtures (e.g., missing scene nodes or incomplete metadata).

### 4) Beam rendering

Peraviz currently supports two beam rendering modes:

- **Volumetric (default)**  
  Designed for a more realistic haze/shaft look with adjustable quality presets.

- **Lightweight (legacy)**  
  A lower-cost cone/mesh approach for maximum FPS on weaker hardware.

### 5) Gobos (parsing + runtime texture loading)

- Peraviz reads gobo wheel data from GDTF and maps DMX values to wheel slots.
- Multiple active gobo wheels can be composed into a single gobo mask texture.
- If gobo media is missing or malformed, Peraviz can generate a simple fake gobo texture to validate DMX bindings.
- Projection/emission paths are intentionally disabled while gobo rendering is being refactored.

### 6) Debug and validation workflow

Peraviz includes explicit tooling to make import correctness reproducible:

- **Coordinate-system audit**
  - Debug mode prints deterministic metadata about the mapping (scale, up/forward axes, handedness, beam local reference).

- **Baseline render validation**
  - A checklist-driven workflow to ensure changes do not introduce unintended transform/scale regressions across representative test scenes.

## Build and run

### Requirements

- **Godot**: 4.2+
- **godot-cpp**: compatible with the chosen Godot version (the project targets the 4.2.x line)
- Native dependencies used by the loader:
  - `tinyxml2`
  - `wxWidgets` (used for reading MVR ZIP containers via `wxZipInputStream`)

### Build the native extension (GDExtension)

The native project lives under `peraviz/native/`.

Example build (from repository root):

```bash
cmake -S peraviz/native -B peraviz/native/build -DCMAKE_BUILD_TYPE=Debug
cmake --build peraviz/native/build --config Debug
```

The resulting library is copied into `peraviz/bin/` so the Godot editor can load it.

#### Root CMake integration

The repository root CMake supports enabling/disabling the Peraviz native build via:

- `-DPERAVIZ_ENABLE_NATIVE=ON` (default)
- Disable with: `-DPERAVIZ_ENABLE_NATIVE=OFF`

### Run the viewer

1. Open the Godot project located in `peraviz/`.
2. Ensure the native extension is built and present in `peraviz/bin/`.
3. Run the test scene (or the main viewer scene) and load an `.mvr` file from disk.
4. (Optional) Enable DMX, connect an Art-Net source, and verify incoming universes in the DMX monitor.

## Known limitations and roadmap direction

- **GDTF quality matters**  
  Peraviz is intentionally strict about following the information present in GDTF. If a GDTF is incomplete or inconsistent (missing gobo media, unclear channel mapping), behavior may degrade. Debug tooling exists to help identify these cases.

- **Proxy-first geometry**  
  The current milestone prioritizes correct transforms and DMX-driven behavior. High-fidelity model reuse (materials, full fixture geometry pipelines) is a likely future improvement.

- **Independence vs integration**  
  Peraviz is expected to remain a dedicated viewer. Any future integration with Perastage should be modular and optional.

## Additional documentation

More detailed notes live under `peraviz/docs/`:

- `NATIVE_BUILD.md` – building the GDExtension
- `COORDINATE_SYSTEM.md` – coordinate mapping and auditing strategy
- `BASELINE_MVR_RENDER.md` – baseline comparison workflow
- `BEAM_RENDERING_MODES.md` – beam modes and performance guidance
- `LIGHT_INTENSITY_INTERPRETATION.md` – interpreting intensity/exposure in the viewer
- `UI_ARCHITECTURE.md` – UI module map, visibility tiers, naming, and ownership rules
- `UI_GROWTH_CHECKLIST.md` – pre-feature UI checklist, including clean-screen default gate
- `README_gobos.md` – gobo parsing and runtime projection notes
