# Perastage – MVR/GDTF Scene Viewer and Rider Importer

**Perastage** is a cross‑platform viewer and editor for *My Virtual Rig (MVR)* scenes and the *General Device Type Format (GDTF)*.  It is designed for lighting designers and technicians who need to inspect, patch and export shows.  Written in modern C++20 using **wxWidgets** for the UI and **OpenGL** for rendering, the application runs on Windows, Linux and macOS and is released under the GPL v3 licence.

## Features

- **Import MVR 1.6 files** – load complete scenes including fixtures, trusses and generic objects.
- **Rider import** – parse simple technical riders (plain text or PDF) to create dummy fixtures and trusses 【889267130405454†L21-L26】.
- **3D and 2D viewers** – inspect the rig from any angle with the OpenGL‑based 3D view or switch to a top‑down 2D plan.
- **Fixture, truss and object tables** – browse all elements of the scene in sortable, searchable lists and export them to CSV.
- **Console panel** – view log messages, warnings and errors in a built‑in console.
- **Layer and summary panels** – organise objects into layers and view a summary of universe usage and object counts.
- **Auto‑patching** – automatically assign DMX universes and channels to fixtures grouped by hang position and type 【472218670938363†L21-L30】.
- **Random colouring** – apply random colours to fixtures to aid visual distinction.
- **Search and download GDTF fixtures** – look up fixtures in the GDTF library (requires internet access) and insert them into the scene.
- **Project management** – create new projects, load existing ones, save your work and export to MVR or CSV.
- **Export tools** – export selected trusses, objects or entire scenes back to MVR or as CSV lists.
- **Integrated help** – view this document and other help files inside the application via the built‑in Markdown viewer.

## Build prerequisites

Perastage uses CMake to manage its build.  You need a C++20‑capable compiler and the following libraries:

- **wxWidgets** (built with the OpenGL, AUI and HTML components) – cross‑platform GUI toolkit.
- **tinyxml2** – XML parser for reading MVR and GDTF files.
- **OpenGL** and **GLEW** – for rendering the 3D and 2D views.
- **libcurl** – used by the GDTF downloader.
- **nanovg** – vector graphics library for 2D drawing.
- **PoDoFo** – used to extract text from PDF rider files.

On Windows and Linux the easiest way to fetch and build these dependencies is to run the provided setup scripts:

```bash
# Linux/macOS
./setup.sh

# Windows (PowerShell)
./setup_windows.ps1
```

These scripts use `vcpkg` or system package managers to install the necessary libraries, configure CMake and build the Perastage binary in a `build` folder.  You can also configure and build manually:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The resulting executable (`Perastage` on Linux/macOS or `Perastage.exe` on Windows) will be placed in the build output directory along with `resources`, `help.md`, `LICENSE.txt` and other runtime files.

## Usage

1. **Run the application** by launching the built executable.  The first time it runs it creates a preferences file in your home directory.
2. **Import a scene** via *File → Import MVR…* to open an existing MVR 1.6 file, or use *File → Import Rider…* to parse a technical rider (plain text or PDF) and automatically create dummy fixtures and trusses.
3. **Navigate the scene** using the 3D or 2D viewer tabs.  The 3D viewer supports orbit (left mouse), pan (middle mouse) and zoom (scroll wheel) controls.  Keyboard shortcuts like **W/A/S/D** move the camera; check the *Help → Keyboard Shortcuts* menu for details.
4. **Inspect the scene** using the *Fixtures*, *Trusses* and *Objects* tables.  You can sort by any column, search for items, and export the current view to CSV.
5. **Auto‑patch your fixtures** by clicking *Tools → Auto‑Patch*.  This feature groups fixtures by hang position and type, then assigns universes and channels in order to minimise patch conflicts【472218670938363†L21-L30】.
6. **Randomise fixture colours** via *Tools → Random Colour* to improve visual differentiation.
7. **Add or edit fixtures** through the *Add Fixture* button or by double‑clicking a row in the fixtures table.  You can choose a fixture type from the downloaded GDTF dictionary, assign positions, patch information and colour.
8. **Export your work** using *File → Export to MVR* to save the current project as an MVR file, or *File → Export CSV* to export the tables for further processing.
9. **Save projects** and reopen them later via *File → Save Project* and *File → Open Project*; Perastage stores project data in a portable JSON format inside the `library/projects` folder.

## Running the tests

This repository includes a suite of Catch2 unit tests under the `tests` directory.  After configuring the build with CMake, you can run them using ctest:

```bash
cmake --build build --target tests
cd build
ctest
```

Tests cover key functionality such as auto‑patch grouping, rider import ordering and PDF text extraction.

## Authors and license

Perastage was created by **Peramato**.  The project is released under the [GNU General Public License v3.0](./LICENSE.txt).  Third‑party libraries bundled with the project are licensed under their respective licences (see `THIRD_PARTY_LICENSES.md` and the `licenses` folder).

We welcome contributions!  Feel free to open issues or pull requests on GitHub for bug reports, feature requests or improvements.
