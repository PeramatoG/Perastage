# Perastage

Perastage is a cross-platform viewer for **MVR** (My Virtual Rig) scenes based on
the [GDTF](docs/gdtf-spec.md) standard. It lets you inspect fixtures, trusses and
other objects in a 3D viewport and provides tabular views of all parsed
elements. Perastage soporta modelos 3D en formatos **3DS** y **GLB** cuando se
referencian dentro de archivos MVR o GDTF. Las posiciones de colgado (Hang
Positions) definidas en los nodos `<Position>` de MVR se importan y se muestran
en las tablas de fixtures y trusses.

## Features

- Importación de escenas **MVR 1.6** y riders en `.txt` o `.pdf`.
- Visores simultáneos **3D** y **2D** con disposición personalizable.
- Tablas de Fixtures, Trusses y Objetos con edición y detección de conflictos de patch.
- Panel de **consola** para comandos y edición numérica de posiciones y rotaciones.
- Paneles de **capas** y **resumen** para organizar y revisar el estado de la escena.
- Gestión de proyectos: crear, cargar, guardar y exportar a MVR, CSV o impresión.
- Herramientas para descargar fixtures GDTF, exportar fixtures/trusses/objetos y **auto‑patch** de direcciones.

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
**File → Import Rider** to parse basic fixture/truss information from `.txt`
o `.pdf` riders (un ejemplo de PDF se incluye en `docs/`). Las tablas de
Fixtures, Trusses y Objects se rellenan automáticamente y la escena se muestra
en el visor 3D. Paneles adicionales —como consola, capas o resumen— se pueden
activar desde el menú **View** y la disposición de paneles se guarda entre
sesiones.

La tabla de fixtures marca en rojo los conflictos de patch cuando dos fixtures
comparten universo y rango de canales. El comando **Tools → Auto patch**
asigna direcciones automáticamente a las fixtures seleccionadas. Los datos
pueden exportarse a MVR, CSV o impresión mediante las opciones del menú **File**.

La opción **Tools → Download GDTF fixture** descarga fixtures directamente de
[GDTF‑Share](https://gdtf-share.com) usando la API oficial (`login.php` +
`getList.php`). Solo es necesario introducir usuario y contraseña una vez; se
almacenan con el proyecto. La descarga depende de la herramienta de línea de
comandos `curl`.

### Ejemplo rápido

1. Importa un archivo `.mvr` con **File → Import MVR**.
2. Pulsa **1** para mostrar la tabla de fixtures y selecciona varios elementos.
3. Ejecuta **Tools → Auto patch** para asignarles direcciones.
4. Exporta la tabla con **File → Export CSV** para obtener un listado de patch.
5. Navega por la escena con los atajos de teclado descritos abajo.

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

## Licencias de terceros

Perastage uses third-party libraries under permissive licenses that are compatible with GPL v3. Consulta [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) para ver las licencias de las dependencias incluidas.
