# Compilación nativa (GDExtension) para Peraviz

Este documento describe cómo compilar la primera extensión nativa C++ (`peraviz_native`) para el proyecto Godot `peraviz/`.

## Versiones soportadas (hito actual)

- **Godot**: `4.2+` (probado con configuración de proyecto Godot 4.x).
- **godot-cpp**: `godot-4.2.2-stable` (se obtiene automáticamente por `FetchContent` si no se indica ruta local).

## Estructura relevante

- `peraviz/native/`: código fuente y CMake de la extensión.
- `peraviz/peraviz.gdextension`: descriptor que Godot carga para enlazar con la biblioteca.
- `peraviz/bin/`: salida de la biblioteca compilada (`.dll` / `.so` / `.dylib`).

## Dependencias

- CMake `>= 3.21`
- Compilador C++17 compatible
  - Linux: GCC/Clang
  - Windows: MSVC (Visual Studio 2022 recomendado)
  - macOS: AppleClang
- Git (si se usa `FetchContent` para descargar `godot-cpp`)

## Compilación standalone (recomendada para este hito)

Desde la raíz del repositorio:

```bash
cmake -S peraviz/native -B peraviz/native/build -DCMAKE_BUILD_TYPE=Debug
cmake --build peraviz/native/build --config Debug
```

La librería resultante se copia automáticamente a `peraviz/bin/`.

## Usar `godot-cpp` local (opcional)

Si prefieres no descargar `godot-cpp` en el configure:

```bash
cmake -S peraviz/native -B peraviz/native/build \
  -DGODOT_CPP_DIR=/ruta/a/godot-cpp \
  -DPERAVIZ_FETCH_GODOT_CPP=OFF
cmake --build peraviz/native/build --config Debug
```

## Integración con CMake raíz

El `CMakeLists.txt` de la raíz incluye `peraviz/native/` mediante la opción:

- `-DPERAVIZ_ENABLE_NATIVE=ON` (por defecto).

Si hace falta desactivarlo temporalmente:

```bash
cmake -S . -B build -DPERAVIZ_ENABLE_NATIVE=OFF
```

## Verificación en Godot

1. Abrir `peraviz/project.godot` con Godot 4.x.
2. Ejecutar el proyecto.
3. Revisar la consola:
   - Mensaje nativo: `[PeravizNative] pong from Peraviz native C++`
   - Mensaje de script: `[PeravizTest] pong from Peraviz native C++`

Eso confirma que GDScript puede invocar el método `ping()` expuesto por C++ vía GDExtension.
