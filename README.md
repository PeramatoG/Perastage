# Perastage

<p align="center">
  <a href="https://github.com/PeramatoG/Perastage/releases/latest">
    <img alt="Latest Release" src="https://img.shields.io/github/v/release/PeramatoG/Perastage?label=release&style=flat-square">
  </a>
  <a href="https://github.com/PeramatoG/Perastage/releases/latest">
    <img alt="Latest Release Downloads" src="https://img.shields.io/github/downloads/PeramatoG/Perastage/latest/total?label=latest%20downloads&style=flat-square">
  </a>
  <a href="https://github.com/PeramatoG/Perastage/releases">
    <img alt="Total Downloads" src="https://img.shields.io/github/downloads/PeramatoG/Perastage/total?label=total%20downloads&style=flat-square">
  </a>
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square">
  <img alt="wxWidgets" src="https://img.shields.io/badge/wxWidgets-3.3.1-green?style=flat-square">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue?style=flat-square">
  <a href="https://github.com/PeramatoG/Perastage/blob/main/LICENSE.txt">
    <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square">
  </a>
</p>

![Perastage 3D View](resources/perastage3d.png)

**Perastage is a free, cross-platform desktop application for viewing, editing, and exporting MVR-based stage files.**

It is designed to open an MVR project quickly, inspect its contents in a clear visual way, and make it easier to review fixtures, trusses, hoists, objects, and general scene structure without needing a full real-time DMX visualizer.

Perastage is **not** a real-time DMX visualizer. Its main purpose is to provide a fast and practical way to view, check, and work with MVR files that use GDTF libraries.

**Help website:** https://perastage.luismaperamato.com/  
**Latest release:** https://github.com/PeramatoG/Perastage/releases/latest  

## What Perastage is for

Perastage focuses primarily on **viewing and working with MVR files**. It uses **GDTF fixture libraries** to represent lighting devices and supports personal dictionary workflows so you can build and maintain your own GDTF library.

If you have a free GDTF Share account, Perastage can also connect to the official API to download GDTF files and help you complete your local library.

The goal is simple: make MVR files easy to open, understand, review, and present.

## Highlights

- Fast and practical **MVR viewer** for lighting and show files.
- **Free and cross-platform** desktop application.
- Uses **GDTF libraries** to represent fixture data.
- Can connect to the **official GDTF API** to download fixture profiles if you have an account.
- Lets you maintain your own **custom GDTF dictionary/library**.
- Includes both **3D viewing** and **plan-focused 2D workflows**.
- Supports importing and exporting MVR scenes.
- Can create elements directly from text using **Tools → Create from text**.
- Includes some distinctive workflow tools, such as the ability to **create, modify, adjust, and distribute MVR elements from the command line**.
- Helps review scene content quickly in a comfortable visual way.

## Installation

The recommended way to install Perastage is to download the latest release from GitHub:

- Go to the **latest release** in this repository and download the package for your platform.
- On Linux, the AppImage remains the recommended generic package. Experimental Arch Linux packages may also be attached for Arch-based distributions and should be tested on real Arch, Manjaro, or EndeavourOS systems.

If you want to build from source, setup and dependency notes are available in the documentation under `docs/`.

Recent 3D optimization updates depend on `meshoptimizer`; if you prepare dependencies manually, include it in your toolchain setup (see `docs/build.md` and `setup_windows.ps1`).

## Basic usage

- Open an existing `.mvr` file to inspect its content quickly.
- Review fixtures, trusses, hoists, objects, and scene structure.
- Use **Tools → Create from text** to generate elements from rider-style notes.
- In **Tools → Create from text**, you can annotate templates with comments
  using inline markers `((...))` or `*(...)*` (ignored by the parser).
- Use the available tools to adjust and organize scene data more comfortably.


## User Guide

**Latest online documentation:** https://perastage.luismaperamato.com/

**Perastage User Guide (GitHub Pages):** [docs/index.html](docs/index.html)

## Documentation

The README is intentionally kept fairly compact. More detailed documentation lives in `docs/`:

- [Documentation policy](docs/documentation_policy.md)
- [Feature overview](docs/features.md)
- [Opening and merging MVR files](docs/opening-mvr-files.md)
- [MVR exporter warning behavior](docs/mvr_exporter.md)
- [MVR-xchange TCP Mode publisher](docs/mvr_xchange.md)
- [Changes since beta 0.1.0](docs/changes_since_beta_0_1_0.md)
- [Build and dependency guide](docs/build.md)
- [Windows installation notes](docs/installation_windows.md)
- [Packaging and platform integration](docs/packaging.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Repository structure](docs/repository_layout.md)
- [Text-to-scene parsing rules](docs/text_to_scene_rules.md)
- [GDTF mutation policy](docs/gdtf_mutation_policy.md)
- [Preferences](docs/preferences.md)
- [GUI shortcut architecture](docs/gui_shortcut_architecture.md)

## Contributing

Feedback is very welcome.

If you try Perastage, it would be especially helpful if you:

- report bugs or unexpected behaviour
- suggest ideas for improving workflows or usability
- share edge cases or MVR/GDTF files that help improve compatibility

This is particularly useful for keeping Perastage as compatible and practical as possible across files created by different applications.

## License

Perastage is distributed under the GNU General Public License v3.0. See [LICENSE.txt](LICENSE.txt).

## Author

Perastage is developed and maintained by **Luisma Peramato** (GitHub: `PeramatoG`).
