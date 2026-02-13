# Native build (GDExtension) for Peraviz

This document explains how to build the C++ native extension (`peraviz_native`) for the Godot project in `peraviz/`, including the initial MVR proxy loading workflow.

## Supported versions

- **Godot**: `4.2+`.
- **godot-cpp**: `godot-4.2.2-stable`.

## New dependencies for the MVR loader

In addition to `godot-cpp`, the native target now links:

- `tinyxml2`: parsing `GeneralSceneDescription.xml`.
- `wxWidgets (core/base)`: reading `.mvr` ZIP containers through `wxZipInputStream`.
- Shared Perastage headers (`models/types.h`, `models/matrixutils.h`) for matrix handling and transform composition.

## Coordinate conversion (MVR/GDTF -> Godot)

The conversion is performed in C++ before exposing data to GDScript:

- MVR typically uses millimeters with a Z-up convention.
- Godot uses meters with a Y-up convention.
- Axis mapping and unit scaling are applied in `mvr_scene_loader.cpp`, and values are exported to GDScript as `Vector3` for position/rotation/scale.

## Standalone build

From the repository root:

```bash
cmake -S peraviz/native -B peraviz/native/build -DCMAKE_BUILD_TYPE=Debug
cmake --build peraviz/native/build --config Debug
```

The resulting library is copied automatically to `peraviz/bin/`.

## Root CMake integration

The root CMake includes this subproject with:

- `-DPERAVIZ_ENABLE_NATIVE=ON` (default).

To disable it:

```bash
cmake -S . -B build -DPERAVIZ_ENABLE_NATIVE=OFF
```

## Usage from Godot

The native class `PeravizLoader` exposes:

- `load_mvr(path: String) -> Array`

Each array element is a `Dictionary` with:

- `id: String`
- `type: String` (`fixture`, `truss`, `support`, `scene_object`)
- `is_fixture: bool`
- `pos: Vector3`
- `rot: Vector3` (degrees)
- `scale: Vector3`

The script `res://scripts/load_scene.gd` consumes this data to instantiate proxy meshes (`ConeMesh` / `BoxMesh`) in the test scene.
