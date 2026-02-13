# Compilación nativa (GDExtension) para Peraviz

Este documento describe cómo compilar la extensión nativa C++ (`peraviz_native`) para el proyecto Godot `peraviz/`, incluyendo la carga inicial de escenas MVR con proxies.

## Versiones soportadas

- **Godot**: `4.2+`.
- **godot-cpp**: `godot-4.2.2-stable`.

## Dependencias nuevas para el cargador MVR

Además de `godot-cpp`, el target nativo ahora enlaza:

- `tinyxml2`: parseo de `GeneralSceneDescription.xml`.
- `wxWidgets (core/base)`: lectura del contenedor ZIP `.mvr` mediante `wxZipInputStream`.
- Cabeceras compartidas de Perastage (`models/types.h`, `models/matrixutils.h`) para matrices y composición de transformaciones.

## Conversión de coordenadas (MVR/GDTF -> Godot)

La conversión se hace en C++ antes de exponer datos a GDScript:

- MVR trabaja típicamente en mm con convención Z-up.
- Godot usa metros con convención Y-up.
- Se aplica mapeo de ejes y escala de unidades en `mvr_scene_loader.cpp`, y se exporta a GDScript como `Vector3` para posición/rotación/escala.

## Compilación standalone

Desde la raíz del repositorio:

```bash
cmake -S peraviz/native -B peraviz/native/build -DCMAKE_BUILD_TYPE=Debug
cmake --build peraviz/native/build --config Debug
```

La librería resultante se copia automáticamente a `peraviz/bin/`.

## Integración con CMake raíz

La raíz incluye el subproyecto con:

- `-DPERAVIZ_ENABLE_NATIVE=ON` (por defecto).

Para desactivar:

```bash
cmake -S . -B build -DPERAVIZ_ENABLE_NATIVE=OFF
```

## Uso desde Godot

La clase nativa `PeravizLoader` expone:

- `load_mvr(path: String) -> Array`

Cada elemento del array es un `Dictionary` con:

- `id: String`
- `type: String` (`fixture`, `truss`, `support`, `scene_object`)
- `is_fixture: bool`
- `pos: Vector3`
- `rot: Vector3` (grados)
- `scale: Vector3`

El script `res://scripts/load_scene.gd` usa estos datos para instanciar proxies (`ConeMesh` / `BoxMesh`) en la escena de prueba.
