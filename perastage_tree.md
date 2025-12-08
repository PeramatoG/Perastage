# Perastage file tree and descriptions

This document provides a high‑level overview of the files and directories in the **Perastage** repository and a brief description of the purpose of each file.  The project is organised into several directories: `core` (core logic and utilities), `gui` (wxWidgets‑based user interface components), `models` (data structures representing the 3D scene and its objects), `mvr` (parsers/exporters for the MVR format), `viewer3d` and `viewer2d` (3D and 2D viewers), `tests` (unit tests), `docs` (external specifications), `library` (built‑in fixtures/trusses/misc assets), `licenses` (third‑party licences), and top‑level build and script files.

```
Perastage/ (root)
├── .gitattributes – Git attributes for line endings/diff settings.
├── README.md – Original project overview and usage instructions.
├── help.md – Help document displayed via the in‑app help menu.
├── setup.sh – Script that fetches dependencies and builds the project on Linux/macOS.
├── setup_windows.ps1 – PowerShell script that sets up and builds the project on Windows.
├── main.cpp – The program entry point; initializes wxWidgets and starts the Perastage application.
├── CMakeLists.txt – CMake build configuration (defines sources and dependencies).
├── LICENSE.txt – GPL v3 licence for Perastage.
├── THIRD_PARTY_LICENSES.md – List of licences for bundled third‑party libraries.
├── .gitattributes – Ensures consistent line endings and diff handling.
├── resources/
│   └── Perastage.rc – Windows resource file containing the application icon and version info.
├── library/
│   ├── fixtures/
│   │   └── gdtf_dictionary.json – Predefined mapping of fixture names to GDTF URLs used by the fixture search dialog.
│   ├── trusses/ – Default truss definitions used when importing MVR scenes or riders.
│   ├── misc/ – Miscellaneous assets (e.g. textures) distributed with the app.
│   ├── scene objects/ – Default 3D models for generic scene objects.
│   └── projects/ – Example project files demonstrating Perastage usage.
├── licenses/
│   ├── nanovg_LICENSE.txt – Licence for the NanoVG vector graphics library.
│   ├── wxwidgets_LICENSE.txt – Licence for the wxWidgets GUI toolkit.
│   ├── glew_LICENSE.txt – Licence for the GLEW OpenGL extension loader.
│   ├── nlohmann_json_LICENSE.txt – Licence for the JSON parser used in the project.
│   ├── curl_LICENSE.txt – Licence for libcurl, used to download GDTF files.
│   ├── stb_easy_font_LICENSE.txt – Licence for stb_easy_font (simple bitmap font for OpenGL).
│   └── … – Other third‑party licences bundled with the application.
├── docs/
│   ├── gdtf-spec.md – Summary of the General Device Type Format (GDTF) specification.
│   └── mvr-spec.md – Overview of the My Virtual Rig (MVR) format.
├── core/ – Core logic for parsing, patching and utilities.
│   ├── autopatcher.h / autopatcher.cpp – Automatically assigns DMX addresses to fixtures grouped by hang position and type【472218670938363†L21-L30】.
│   ├── patchmanager.cpp – Helper functions used by the auto‑patcher to group fixtures and manage universe/channel allocation.
│   ├── riderimporter.h / riderimporter.cpp – Reads simple rider files (plain text or PDF) to generate dummy fixtures and trusses【889267130405454†L21-L26】.
│   ├── pdftext.h / pdftext.cpp – Extracts plain text from PDF rider documents using PoDoFo.
│   ├── projectutils.h / projectutils.cpp – Functions for loading, saving and exporting project files.
│   ├── gdtfnet.h / gdtfnet.cpp – Downloads GDTF fixture definitions over HTTP using libcurl.
│   ├── gdtfdictionary.cpp – Loads the fixture dictionary JSON file from the library.
│   ├── markdown.h / markdown.cpp – Converts Markdown (e.g., `help.md`) into HTML for the built‑in help viewer.
│   ├── logger.h / logger.cpp – Simple logging utility used throughout the application.
│   ├── stringutils.h – Helper functions for string manipulation and parsing.
│   ├── simplecrypt.h – Lightweight encryption helper used for storing credentials in preferences.
│   ├── configmanager.h – Defines the configuration manager class for storing user preferences.
│   ├── trussloader.h – Functions to load truss definitions from the library.
│   └── … – Additional helpers and utility classes supporting the core functionality.
├── gui/ – Graphical user interface components built with wxWidgets.
│   ├── mainwindow.h / mainwindow.cpp – Defines the main application window and menus.
│   ├── layerpanel.h / layerpanel.cpp – Panel listing scene layers and allowing them to be toggled on/off.
│   ├── fixturetablepanel.h / fixturetablepanel.cpp – Table view that lists all fixtures and their properties.
│   ├── sceneobjecttablepanel.cpp – Table view listing generic objects in the scene.
│   ├── summarypanel.h / summarypanel.cpp – Shows a summary of the loaded scene (object counts, universe usage, etc.).
│   ├── consolepanel.h / consolepanel.cpp – Text console for logs, errors and informational messages.
│   ├── tableprinter.h / tableprinter.cpp – Utility to print or export the current table view to CSV.
│   ├── splashscreen.h / splashscreen.cpp – Displays a splash screen while the application loads.
│   ├── preferencesdialog.cpp – Dialog allowing users to configure preferences (paths, default units, etc.).
│   ├── exporttrussdialog.h / exporttrussdialog.cpp – Dialog to export selected trusses to an MVR file.
│   ├── exportobjectdialog.h / exportobjectdialog.cpp – Dialog to export generic objects.
│   ├── fixtureeditdialog.h / fixtureeditdialog.cpp – Dialog for editing fixture properties (name, type, patch, etc.).
│   ├── fixturepatchdialog.h / fixturepatchdialog.cpp – Dialog to manually assign DMX universes and channels to fixtures.
│   ├── addressdialog.h / addressdialog.cpp – Dialog to specify DMX addresses for new fixtures.
│   ├── selectfixturetypedialog.cpp – Dialog for choosing a fixture type from the GDTF dictionary when adding a fixture.
│   ├── columnselectiondialog.h – Dialog to choose which columns to display in tables.
│   ├── gdtfsearchdialog.h – Declaration for a dialog that searches the online GDTF database and downloads fixtures.
│   ├── fixturepreviewpanel.cpp – Panel that shows a preview of a fixture’s 3D geometry.
│   ├── logindialog.cpp – (Optional) stub for a future login system using encrypted credentials.
│   └── … – Other panels and dialogs supporting the user interface.
├── models/ – Data structures representing the scene and its objects.
│   ├── mvrscene.h / mvrscene.cpp – Holds the complete MVR scene with fixtures, trusses and generic objects.
│   ├── layer.h / layer.cpp – Represents a scene layer used to group objects.
│   ├── sceneobject.h / sceneobject.cpp – Base class for any 3D object with position, rotation and scale.
│   ├── fixture.h / fixture.cpp – Describes a lighting fixture, including manufacturer, model and patch information.
│   ├── truss.h / truss.cpp – Describes a truss element with length, height and mounting points.
│   ├── types.h – Defines common enums and type aliases used across the models.
│   └── matrixutils.h – Helper functions for matrix and vector operations used by viewers.
├── mvr/ – Parsing and exporting the MVR format.
│   ├── mvrimporter.h – Interface for importing MVR 1.6 scenes into Perastage.
│   ├── mvrimporter.cpp – Implementation of the MVR importer (creates fixtures, trusses and objects from MVR).
│   ├── mvrexporter.cpp – Exports the current scene back into an MVR file for use in other software.
│   └── … – Additional helpers for handling the MVR file structure.
├── viewer3d/ – OpenGL‑based 3D renderer.
│   ├── viewer3dcontroller.h – Manages the 3D scene, updates and input handling.
│   ├── viewer3dpanel.h / viewer3dpanel.cpp – wxWidgets panel that renders the 3D scene.
│   ├── viewer3dcamera.h – Controls camera position, orientation and zoom in the 3D view.
│   ├── loader3ds.cpp – Loader for legacy 3D Studio (.3ds) mesh files used by some truss models.
│   ├── loaderglb.cpp – Loader for glTF‑binary (.glb) mesh files used by fixtures.
│   ├── gdtfloader.cpp – Loads fixture geometry from GDTF files for preview and rendering.
│   ├── mesh.h – Defines a generic mesh structure (vertices, textures, normals, etc.).
│   └── … – Additional rendering helpers and shaders.
├── viewer2d/ – 2D plan view renderer.
│   ├── viewer2dpanel.h / viewer2dpanel.cpp – Panel that draws a top‑down view of the scene.
│   └── viewer2drenderpanel.h – Renders 2D vector drawings of fixtures and trusses.
├── tests/ – Unit tests using Catch2.
│   ├── autopatcher_grouping_test.cpp – Verifies the grouping logic used by the auto‑patcher.
│   ├── rider_import_order_test.cpp – Ensures riders are imported in the correct order and positions.
│   ├── pdf_text_test.cpp – Tests the PDF text extraction used by the rider importer.
│   ├── consolepanel_stub.cpp – Stub implementation for the console panel used in tests.
│   ├── riderimporter_stubs.cpp – Stub classes for testing the rider importer in isolation.
│   ├── trussloader_stub.cpp – Stub classes for testing the truss loader.
│   └── … – Additional tests verifying core functionality.
```

Each entry above summarises the role of the file or folder; for example, the `autopatcher` source automatically assigns DMX addresses to fixtures 【472218670938363†L21-L30】, and the `riderimporter` reads simple rider files to create dummy fixtures and trusses 【889267130405454†L21-L26】.  Other source files follow similar patterns—header (`*.h`) files declare classes and functions, while implementation (`*.cpp`) files define them.  The GUI directory contains numerous dialogs and panels built with wxWidgets, the models directory defines the scene data structures, and the viewer directories contain the 3D and 2D renderers.
