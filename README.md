# Perastage

Perastage is a cross-platform viewer for **MVR** (My Virtual Rig) scenes based on the
[GDTF](docs/gdtf-spec.md) standard.  It lets you inspect fixtures, trusses and other
objects in a 3D viewport and provides table views of all parsed elements.

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
