# Perastage

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Build: CMake](https://img.shields.io/badge/Build-CMake-064F8C)

![Perastage 3D View](resources/perastage3d.png)

Perastage is a C++20 desktop application for professional lighting design and show documentation. It helps lighting designers, programmers, and technicians import MVR data, manage fixtures/trusses/hoists, and generate printable 2D/3D documentation from a single project. The current stable line extends the original beta workflow set with production-ready MVR, dictionary portability, and layout/print pipelines.

## Highlights

- Imports and exports MVR scenes with integrated GDTF fixture handling and archive-safe asset references.
- Combines a real-time 3D viewer, a plan-focused 2D viewer, and a multi-page layout/print workflow.
- Includes rider/text parsing to generate fixtures and truss structures directly from production notes.
- Provides fixture, truss, hoist, and object tables with batch-edit and patch management workflows.
- Supports flexible dictionary workflows (JSON snapshots, copied assets, and portable ZIP bundles).
- Produces documentation outputs such as layout PDFs, table exports, and print-ready sheets.

## Installation

```bash
git clone https://github.com/PeramatoG/perastage.git
cd perastage
cmake -S . -B build
cmake --build build --config Release
```

For platform-specific setup, dependency installation, and packaging, see [docs/build.md](docs/build.md).

## Basic Usage

- Start the executable from your build output (`build/.../Perastage` or `Perastage.exe`).
- Open an existing `.pstg`/`.mvr` project or create a new scene from the main menu.
- Use **Tools → Create from text** to generate fixtures from rider-style notes, or use the table/layout panels to document the rig.

## Documentation

The README is intentionally concise. Detailed documentation lives in `docs/`:

- [Documentation policy](docs/documentation_policy.md)
- [Feature overview](docs/features.md)
- [Changes since beta 0.1.0](docs/changes_since_beta_0_1_0.md)
- [Build and dependency guide](docs/build.md)
- [Windows installation notes](docs/installation_windows.md)
- [Packaging and platform integration](docs/packaging.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Repository structure](docs/repository_layout.md)
- [Text-to-scene parsing rules](docs/text_to_scene_rules.md)
- [GDTF mutation policy](docs/gdtf_mutation_policy.md)
- [GUI shortcut architecture](docs/gui_shortcut_architecture.md)

## Contributing

Contributions are welcome. Please keep changes modular and update the matching document in `docs/` when behavior changes. If you modify parsing or shortcut behavior, also update the dedicated rule documents referenced above.

## License

Perastage is distributed under the GNU General Public License v3.0. See [LICENSE.txt](LICENSE.txt).

## Author

Perastage is developed and maintained by **Luisma Peramato** (GitHub: `PeramatoG`).
