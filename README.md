# Perastage

Perastage is a cross-platform viewer for **MVR** (My Virtual Rig) scenes based on
the [GDTF](docs/gdtf-spec.md) standard. It lets you inspect fixtures, trusses and
other objects in a 3D viewport and provides tabular views of all parsed elements.
Perastage supports 3D models in **3DS** and **GLB** formats when referenced inside
MVR or GDTF files. Hanging positions defined in MVR `<Position>` nodes are
imported and displayed in the fixture and truss tables.

## Features

- Import **MVR 1.6** scenes and riders in `.txt` or `.pdf` format.
- Simultaneous **3D** and **2D** viewers with a customizable layout.
- Fixture, Truss and Object tables with editing and patch conflict detection.
- **Console** panel for commands and numeric editing of positions and rotations.
- **Layer** and **summary** panels to organize and review the scene state.
- Project management: create, load, save and export to MVR, CSV or print.
- Tools to download GDTF fixtures, export fixtures/trusses/objects, perform
  address **auto‑patching** and assign random colors.

## Build prerequisites

* CMake **3.21** or newer
* A C++20 compiler
* [wxWidgets](https://www.wxwidgets.org/) with AUI and OpenGL components
* [tinyxml2](https://github.com/leethomason/tinyxml2)
* OpenGL development libraries

Packages can be installed manually or via [vcpkg](https://github.com/microsoft/vcpkg).
Setup scripts (`setup.sh` for Linux and `setup_windows.ps1` for Windows) are provided
as examples of how to fetch the dependencies and build the project using CMake.

## Building

```bash
cmake -S . -B build
cmake --build build
```

The commands above produce the `Perastage` executable in the `build` directory.
Runtime assets from the `resources` folder are automatically copied next to the
binary when building.

## Usage

Run the executable and use **File → Import MVR** to load an `.mvr` file or
**File → Import Rider** to parse basic fixture/truss information from `.txt` or
`.pdf` riders (a sample PDF is included in `docs/`). The Fixtures, Trusses and
Objects tables are filled automatically and the scene is shown in the 3D viewer.
Additional panels—such as console, layers or summary—can be enabled from the
**View** menu and the panel layout is saved between sessions.

The Fixtures table highlights patch conflicts in red when two fixtures share a
universe and channel range. The **Tools → Auto patch** command assigns addresses
automatically to the selected fixtures. **Tools → Auto color** applies random
colors per fixture type and assigns light gray to layers prefixed with "truss".
Data can be exported to MVR, CSV or printed using
the options in the **File** menu.

The **Tools → Download GDTF fixture** option downloads fixtures directly from
[GDTF‑Share](https://gdtf-share.com) using the official API (`login.php` +
`getList.php`). You only need to enter your username and password once; they are
stored with the project. Downloading relies on the `curl` command-line tool.

### Quick Example

1. Import an `.mvr` file with **File → Import MVR**.
2. Press **1** to display the Fixtures table and select several items.
3. Run **Tools → Auto patch** to assign addresses.
4. Use **Tools → Auto color** if you want colors by fixture type and light gray
   truss layers.
5. Export the table with **File → Export CSV** to obtain a patch list.
6. Navigate the scene using the keyboard shortcuts described below.

## Keyboard controls

### Global

- **Ctrl+N**: new project.
- **Ctrl+L**: load project.
- **Ctrl+S**: save project.
- **Ctrl+Q**: close the application.
- **Ctrl+Z / Ctrl+Y**: undo / redo.
- **Del**: delete selection.
- **F1**: open help.
- **1/2/3**: show the Fixtures, Trusses or Objects tables.

### 3D viewport

- **Arrow keys**: orbit around the scene.
- **Shift + Arrow keys**: pan the view.
- **Alt + Up/Down** (or **Alt + Left/Right**): zoom in and out.
- **Numpad 1/3/7**: front, right and top views.
- **Numpad 5**: reset to the default orientation.

### 2D viewer

- **Mouse drag**: pan the view.
- **Mouse wheel**: zoom in and out.
- **Arrow keys**: pan the view.
- **Alt + Arrow keys**: zoom in and out.

## License

This project is licensed under the GNU General Public License v3.0 (GPL v3) – see the LICENSE.txt file for details.

## Authors

- Luis Manuel Peramato García (Luisma Peramato)

## Third-Party Licenses

Perastage uses third-party libraries under permissive licenses that are
compatible with GPL v3. See
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for the licenses of the
included dependencies.
