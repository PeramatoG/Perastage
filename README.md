# Perastage

Perastage is a cross-platform viewer for **MVR** (My Virtual Rig) scenes based on the
[GDTF](docs/gdtf-spec.md) standard.  It lets you inspect fixtures, trusses and other
objects in a 3D viewport and provides table views of all parsed elements.
Perastage soporta modelos 3D en formatos **3DS** y **GLB** cuando se
referencian dentro de archivos MVR o GDTF.

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

Run the executable and use **File → Import MVR** to load an `.mvr` file.  The
application will populate tables with fixtures and trusses and render the scene
in the 3D viewport.  Additional view panels can be toggled from the **View** menu.
The **Tools → Download GDTF fixture** command can download a fixture directly
from [GDTF‑Share](https://gdtf-share.com). It authenticates using the official
session-based API (`login.php` + `getList.php`) so you simply provide your user
name (email) and password once and they are stored with the project. The
download step relies on the `curl` command line tool; on Windows make sure
`curl.exe` is invoked instead of the PowerShell `curl` alias.
Quick
instructions are available under **Help → Help** and
licensing information is shown under **Help → About**.

## Keyboard controls

When the mouse pointer is over the 3D viewport you can use the following shortcuts:

- **Arrow keys**: orbit around the scene.
- **Shift + Arrow keys**: pan the view.
- **Alt + Up/Down** (or **Alt + Left/Right**): zoom in and out.
- **Numpad 1/3/7**: front, right and top views.
- **Numpad 5**: reset to the default orientation.
