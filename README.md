# Perastage

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Build: CMake](https://img.shields.io/badge/Build-CMake-064F8C)

![Perastage 3D View](resources/perastage3d.png)

Perastage is a free, cross-platform desktop application for opening, reviewing, and documenting stage scenes from `.mvr` files and Perastage `.pstg` projects.

Perastage focuses on practical scene review, rigging/fixture inspection, and layout documentation. It is not a real-time DMX visualizer.

## Quick start

1. Start Perastage.
2. Open a project with **File > Load** (`.pstg`) or import a scene with **File > Import MVR...** (`.mvr`).
3. Review data in **Fixtures**, **Trusses**, **Hoists**, and **Objects** tables.
4. Use **2D Viewer**, **3D Viewer**, and **Layouts** for visual checks and printable plans.

## Supported formats

- **Project**: `.pstg`
- **Scene import/export**: `.mvr`
- **Text-to-scene input**: rider text and `.pdf` content through **Tools > Create from text...**
- **Fixture profile resources**: `GDTF` (local library and optional online download)

## What you can do

- Open and inspect `.mvr` scenes and `.pstg` projects.
- Navigate scene data in synchronized 3D, 2D, and table views.
- Manage fixtures, trusses, hoists, scene objects, layers, and patch data.
- Import and export MVR for interoperability.
- Use GDTF dictionaries and optional GDTF Share login/download workflows.
- Generate scene content from rider-style text with **Tools > Create from text...**.
- Add primitive scene objects from **Edit > Add basic geometry**.
- Build layout pages and export/print plan documentation.

## Installation

Download the latest package from this repository's Releases page.

For source builds, see the build guide in `docs/build.md`.

## Documentation

- [Perastage Help (in-app manual source)](help.md)
- [Feature overview](docs/features.md)
- [Text-to-scene rules](docs/text_to_scene_rules.md)
- [GUI shortcut architecture](docs/gui_shortcut_architecture.md)
- [Build and dependency guide](docs/build.md)
- [Packaging and platform integration](docs/packaging.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Changes since beta 0.1.0](docs/changes_since_beta_0_1_0.md)

## Contributing and feedback

Feedback, workflow suggestions, and compatibility reports are welcome.

If you share issues or examples, include:

- platform and version,
- sample `.mvr` / `.pstg` where possible,
- steps to reproduce the observed behavior.

## License

Perastage is distributed under the GNU General Public License v3.0. See [LICENSE.txt](LICENSE.txt).
