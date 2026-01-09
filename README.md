# Perastage

Perastage is a C++20 desktop application for lighting designers and technicians.  
It focuses on reading, organizing and visualizing show data using the MVR (My Virtual Rig) format and GDTF (General Device Type Format).

The application is built with **wxWidgets** for the user interface and **OpenGL** for 3D rendering.

---

## Main features

- **Project management**
  - Open and save Perastage projects.
  - Keep fixtures, trusses and scene objects grouped in layers.
  - Manage multiple printable layouts with stored page settings.
  - Central configuration management for paths and user preferences.

- **Fixture, truss, and hoist model**
  - Internal data model for fixtures, trusses, hoists, and generic scene objects.
  - Layer support (visibility and selection per layer).
  - Basic geometry and transform helpers for positioning elements in the scene.

- **MVR / GDTF support**
  - Import existing MVR files into the scene.
  - Export the current scene back to MVR.
  - GDTF dictionary to map textual fixture descriptions to GDTF files.
  - Loader for GDTF geometry for use in the 3D viewer.

- **Rider import**
  - Import fixture lists from text and PDF documents.
  - Parse typical lighting riders into structured fixture data.
  - Tools to help resolve fixture types and patch information.

- **Patch management**
  - Manage DMX universes and addresses for fixtures.
  - Automatic patching logic (autopatcher) to assign addresses based on rules.
  - Dialogs to edit addresses, modes and universes from the GUI.

- **Viewers**
  - **3D viewer** based on OpenGL to inspect imported trusses, fixtures and scene objects.
  - **2D viewer** for plan-style views. The 2D panel can optionally record the
    drawing steps for the next paint pass. Call `Viewer2DPanel::RequestFrameCapture()`
    before the next refresh and retrieve the resulting `CommandBuffer` through
    `GetLastCapturedFrame()` to obtain an ordered list of drawing commands ready
    for vector backends. All primitives drawn in the 2D view (grid, scene geometry,
    axes and labels) are preserved in draw order so an exporter can reproduce the
    exact frame the user saw on screen.
  - Camera and navigation helpers in the 3D viewer.
  - Layout mode with printable 2D view frames and fixture legends.

- **GUI helpers**
  - Tables for fixtures, trusses, hoists, and scene objects.
  - Multi-edit shortcuts in tables, including range interpolation (`1 10`,
    `1 thru 10`) and sequential fills with trailing separators.
  - Layout list and layout preview panels for print layout composition.
  - Summary, rigging, and console panels.
  - Dialogs for preferences, login, fixture type selection, GDTF search, export options, etc.

---

## Repository layout

Only the most important directories are listed here.

- `main.cpp` – Application entry point; initializes wxWidgets and the main window.
- `core/` – Core logic and utilities (configuration, logging, rider importer, autopatcher, dictionaries, PDF/text helpers, markdown conversion, etc.).
- `gui/` – wxWidgets panels, dialogs and the main window; all user-facing widgets live here.
- `models/` – Data model for fixtures, trusses, layers, scene objects and matrices.
- `mvr/` – Importer and exporter for MVR files.
- `viewer3d/` – 3D viewer code, including camera, controller, mesh management and 3D loaders.
- `viewer2d/` – 2D viewer and rendering panels.
- `library/` – User-visible library data (fixtures, trusses, scene objects, projects, misc).
- `resources/` – Application icon, Windows resource script and other runtime resources.
- `docs/` – Notes and informal documentation (for example GDTF/MVR spec notes).
- `tests/` – Unit tests and stubs for core components.
- `licenses/` – License texts for third‑party libraries used by Perastage.
- `CMakeLists.txt` – Top-level CMake build script.

See `docs/rider_benchmark.md` for the reproducible rider import benchmark that
tracks execution time and memory usage on a large synthetic rider.

For a more detailed description of every file, see the separate `perastage_tree.md` document.

---

## Building

Perastage uses **CMake** as its build system and targets C++20.

### Dependencies

The project expects the following libraries to be available on your system:

- wxWidgets (with components: core, base, aui, gl, html)
- tinyxml2
- OpenGL (GL and GLU)
- GLEW
- CURL
- nanovg
- PoDoFo

On Windows you can use vcpkg, MSYS2 or your preferred package manager.  
On Linux and macOS the libraries are usually available through the system package manager or via vcpkg.

### Configure and build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

CMake will automatically:

- Collect all `.cpp` and `.h` files from the main source directories.
- Link the required third‑party libraries.
- Copy the `resources` directory, license information and the help file next to the executable.
- Ensure the runtime `library` subdirectories exist next to the executable (fixtures, trusses, misc, scene objects, projects).

### Running tests

Tests are built as part of the main CMake project:

```bash
cd build
ctest
```

Some tests rely on stub implementations or small sample files located in the `tests` directory.

---

## Status

Perastage is under active development.  
Some parts of the UI and import/export pipeline may change between versions and certain features are still experimental.

If you find a bug or have a suggestion, feel free to open an issue in the repository and describe:

- What you were trying to do.
- The steps to reproduce the problem.
- Any relevant rider, project or MVR files (if possible).

---

## Author

Perastage is developed and maintained by **Luisma Peramato** (GitHub user `PeramatoG`).

---

## License

The project is distributed under the terms described in `LICENSE.txt` in this repository.

Third‑party components are used under their respective licenses; see `THIRD_PARTY_LICENSES.md` and the files in the `licenses` directory for details.
