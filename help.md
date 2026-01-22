# Perastage Help / Ayuda de Perastage

**Language / Idioma:** [English](#english) | [Español](#espanol)

<details open>
<summary><strong>English</strong></summary>

<a id="english"></a>

Perastage is a high-performance, cross-platform viewer for **MVR** (My Virtual Rig)
scenes with 3D rendering, scene analysis, and editable tabular data. It is designed
for lighting professionals and developers who work with **GDTF** and **MVR**.

## Getting Started

1. Launch the application.
2. Use **File → Import MVR...** to load an `.mvr` file.
3. Use the table tabs (**Fixtures**, **Trusses**, **Hoists**, **Objects**) to inspect data.
4. Toggle panels via the **View** menu if any pane is hidden.

## Project Management

Perastage projects (`*.psproj`) store the scene, layouts, and user configuration for a show.
Projects can contain multiple **layout pages** (printable sheets) and can be opened or
saved at any time.

**File → New / Load / Save / Save As...**

- **New** creates a blank project.
- **Load** opens an existing `.psproj` file.
- **Save** stores changes in the current project file.
- **Save As...** writes the project under a new name or location.

### Configuration persistence

Perastage saves and restores user preferences, including:

- Panel visibility states and layout mode selections.
- Last used file paths (projects, exports, downloads).
- 3D camera position/orientation and 2D view state.
- Print and layout settings.

## Keyboard Shortcuts

### Global

| Key Combination | Function |
| --- | --- |
| Ctrl+N | New project |
| Ctrl+L | Load project |
| Ctrl+S | Save project |
| Ctrl+Q | Close application |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Del | Delete selection |
| F1 | Open help |
| 1 / 2 / 3 / 4 | Switch to Fixtures / Trusses / Hoists / Objects |

### 3D Viewer

| Key Combination | Function |
| --- | --- |
| Arrow Keys | Orbit camera |
| Shift + Arrow Keys | Pan camera |
| Alt + Arrow Keys | Zoom in/out |
| Numpad 1 / 3 / 7 | Front / Right / Top views |
| Numpad 5 | Reset camera orientation |

### 2D Viewer

| Key Combination | Function |
| --- | --- |
| Mouse drag | Pan view |
| Mouse wheel | Zoom in/out |
| Arrow Keys | Pan view |
| Alt + Arrow Keys | Zoom in/out |

## Menu Reference

### File

| Menu Option | Description |
| --- | --- |
| New | Create a new project |
| Load | Open an existing project |
| Save | Save the current project |
| Save As... | Save the project under a new name |
| Import MVR... | Load an `.mvr` file |
| Export MVR... | Export the current scene to MVR |
| Print Viewer 2D... | Export the 2D view to PDF |
| Print Layout... | Export the active layout to PDF |
| Print Table... | Print or preview the active table |
| Export CSV... | Export table data to CSV |
| Close | Close the application |

### Edit

| Menu Option | Description |
| --- | --- |
| Undo / Redo | Undo or redo the last action |
| Add fixture... | Insert a new fixture |
| Add truss... | Insert a new truss |
| Add scene object... | Insert a generic scene object |
| Delete | Remove selected items |
| Preferences... | Open the Preferences dialog |

### View

| Menu Option | Description |
| --- | --- |
| Console | Toggle the Console panel |
| Fixtures | Toggle the tables panel |
| 3D Viewport | Toggle the 3D viewport |
| 2D Viewport | Toggle the 2D viewport |
| 2D Render Options | Toggle the 2D render options panel |
| Layers | Toggle the Layers panel |
| Layouts | Toggle the Layouts panel |
| Summary | Toggle the Summary panel |
| Rigging | Toggle the Rigging panel |
| Layout Views | Switch between 3D Layout View, 2D Layout View, and Layout Mode |

### Tools

| Menu Option | Description |
| --- | --- |
| Download GDTF fixture... | Download fixtures from GDTF‑Share |
| Edit dictionaries... | Edit rider dictionaries |
| Create from text... | Import riders from text or PDF |
| Export Fixture... | Export selected fixtures |
| Export Truss... | Export selected trusses |
| Export Scene Object... | Export selected scene objects |
| Auto patch | Assign DMX addresses automatically |
| Auto color | Assign colors to layers and fixtures |
| Convert to Hoist | Convert fixtures to hoists |

## Tables and Panels

### Fixtures / Trusses / Hoists / Objects tables

- Each table lists the parsed objects from the current project.
- Rotation columns are labeled **Roll (X)**, **Pitch (Y)**, **Yaw (Z)** and are
  applied in Z/Y/X order.
- Common context actions include editing, replacement, export, and address
  management where applicable.

### Layers panel

- Shows all layers used by fixtures, trusses, hoists, and scene objects.
- Toggle visibility per layer using the **Visible** column.
- Select a row to set the **active layer** for newly created items.
- Right‑click a layer to assign a custom color. Colors are used in 2D/3D views.

### Layouts panel

- Lists layout pages (printable sheets) available in the project.
- Use **Add**, **Rename**, and **Delete** to manage pages.
- Change page orientation (portrait/landscape) via the context menu.
- Page order follows the list; reorder where supported by dragging.

### Summary and Rigging panels

- **Summary** provides counts and basic statistics per object type.
- **Rigging** aggregates totals and highlights missing weight data.

### 2D Render Options panel

Controls the 2D viewport render mode, grid, and label visibility.

### Console panel

Displays status messages and provides a command‑line interface for quick edits.

### Table editing shortcuts

- **Apply one value to all selected rows:** enter a single number (or text).
- **Interpolate across selections:** enter a start and end value (`1 12` or `1 thru 12`).
- **Sequential values:** end with a trailing space (`100␠`) or a trailing range token (`100 t`).
- **Range separators:** `t` or `thru` with or without spaces (`1t10`, `1 t 10`, `1 thru 10`).
- **Relative edits:** use `++` / `--` (for example `++0.5`, `--15`).

## Layout Mode and Printing

### Layout mode overview

Layout mode lets you build printable pages with 2D views, legends, event tables,
text boxes, and images. The **Layout Views** toolbar and **Layout** toolbar provide
quick access to layout actions and mirror the corresponding menu commands. The
**File** toolbar offers shortcuts for New/Load/Save/Save As and MVR import/export.

### Layout tools (toolbar and menu)

- **Add 2D View**: inserts a 2D plan view onto the page.
- **Add Legend**: inserts a fixture legend.
- **Add Event Table**: adds a tabular summary of events.
- **Add Text**: inserts a text box.
- **Add Image**: inserts an image block.

Arrange elements by dragging and use context menus to edit, delete, and change
z‑order (bring to front / send to back).

### Printing and export (File menu)

- **Print Viewer 2D...** exports the active 2D view to PDF. Settings include page size
  (A3/A4), orientation, grid visibility, and detailed vs schematic footprints.
- **Print Layout...** exports the active layout page to PDF. The dialog lets you pick
  page size and 2D detail level; layout orientation is taken from the page settings.
- **Print Table...** previews the current table for printing. You can choose columns
  before printing (PDF is available via your OS print dialog).
- **Export CSV...** exports the chosen table columns to a `.csv` file.

## Tools

- **Download GDTF fixture...**: search and download fixtures from **GDTF‑Share**.
  A login dialog requests credentials and stores them locally in encrypted form.
- **Edit dictionaries...**: edit fixture and truss dictionaries used by the rider importer.
- **Create from text...**: import lighting riders from text or PDF. Use the dictionary
  editor to resolve fixture type names and map them to GDTF files.
- **Export Fixture / Truss / Scene Object...**: export selected items for reuse.
- **Auto patch**: assign DMX addresses automatically.
- **Auto color**: assign layer colors and fixture colors by type; truss layers default
  to light gray, and existing explicit colors are preserved.
- **Convert to Hoist**: convert selected fixtures to hoists while preserving names
  and positions.

## Console Commands

The Console panel supports precise selection and transform commands for fixtures
and trusses.

### Selection

- `f <ids>`: select fixtures. Uses the current selection and supports `+`/`-`.
- `t <ids>`: select trusses (replaces the current truss selection).
- `clear`: clear all selections.

Ranges and increments are supported:

- `1 thru 10`, `1 t 10`, `1t10`
- `+ 5` or `- 3` inside a selection command

Examples:

```
f 1 4              # select fixtures 1 through 4
f + 6 8            # add fixtures 6 through 8
f - 2              # remove fixture 2
t 1 3              # select trusses 1 through 3
clear              # clear selection
```

### Position and rotation

- `pos x 1.5` sets X to 1.5 m.
- `pos 0 0 5` sets X/Y/Z directly.
- `pos 1,2,3` uses comma‑separated values for X/Y/Z.
- `x ++0.5` moves only the X axis relative to the current position.
- `rot z 90` sets yaw to 90°.
- `rot ++45` applies a relative rotation on all axes.

You can provide two values to interpolate across a selection (e.g. `pos x 0 10`).
Position values are in meters; rotation values are in degrees.

## Preferences

Open **Edit → Preferences...** to configure rider import defaults:

- Auto‑patch after rider import.
- Auto‑create layers by **position** or **fixture type**.
- Default LX heights, positions, and margins (LX1–LX6) in meters.

## File Format Support

- **MVR 1.6**: import/export of fixtures, trusses, hoists, and objects.
- **GDTF**: embedded fixtures and model definitions.
- **3DS / GLB**: 3D model formats referenced by MVR/GDTF.

## Troubleshooting

- If the 3D viewport does not display:
  - Confirm OpenGL libraries are installed.
  - Ensure the MVR contains valid geometry and GDTF references.
- If objects do not appear in tables:
  - Verify the MVR contains proper object definitions.
  - Check the console for warnings or errors.

## Additional Documentation

- MVR Specification: `docs/mvr-spec.md`
- GDTF Specification: `docs/gdtf-spec.md`
- License: `LICENSE.txt` (GPL v3)

</details>

<details>
<summary><strong>Español</strong></summary>

<a id="espanol"></a>

Perastage es un visor multiplataforma de alto rendimiento para escenas **MVR**
(My Virtual Rig) con renderizado 3D, análisis de escena y tablas editables. Está
pensado para profesionales de iluminación que trabajan con **GDTF** y **MVR**.
En esta guía se usan términos técnicos en inglés: *fixture* (luminaria), *truss*
(estructura) y *hoist* (motor/soporte).

## Primeros pasos

1. Inicia la aplicación.
2. Usa **File → Import MVR...** para cargar un archivo `.mvr`.
3. Revisa los datos en las pestañas **Fixtures**, **Trusses**, **Hoists** y **Objects**.
4. Activa o desactiva paneles desde el menú **View** si faltan en pantalla.

## Gestión de proyectos

Los proyectos de Perastage (`*.psproj`) guardan la escena, los layouts y la
configuración de usuario. Un proyecto puede contener varias **páginas de layout**
(hojas imprimibles) y se pueden abrir o guardar en cualquier momento.

**File → New / Load / Save / Save As...**

- **New** crea un proyecto vacío.
- **Load** abre un archivo `.psproj` existente.
- **Save** guarda los cambios en el proyecto actual.
- **Save As...** guarda el proyecto con otro nombre o ubicación.

### Persistencia de configuración

Perastage guarda y restaura preferencias de usuario, como:

- Visibilidad de paneles y selecciones de modo de layout.
- Últimas rutas usadas (proyectos, exportaciones, descargas).
- Posición/orientación de la cámara 3D y estado de la vista 2D.
- Ajustes de impresión y layouts.

## Atajos de teclado

### Globales

| Tecla | Función |
| --- | --- |
| Ctrl+N | Nuevo proyecto |
| Ctrl+L | Cargar proyecto |
| Ctrl+S | Guardar proyecto |
| Ctrl+Q | Cerrar aplicación |
| Ctrl+Z / Ctrl+Y | Deshacer / Rehacer |
| Del | Borrar selección |
| F1 | Abrir ayuda |
| 1 / 2 / 3 / 4 | Cambiar a Fixtures / Trusses / Hoists / Objects |

### Visor 3D

| Tecla | Función |
| --- | --- |
| Flechas | Orbitar cámara |
| Shift + flechas | Desplazar cámara |
| Alt + flechas | Zoom |
| Numpad 1 / 3 / 7 | Vistas Frente / Derecha / Superior |
| Numpad 5 | Reiniciar orientación de cámara |

### Visor 2D

| Tecla | Función |
| --- | --- |
| Arrastrar con ratón | Desplazar vista |
| Rueda del ratón | Zoom |
| Flechas | Desplazar vista |
| Alt + flechas | Zoom |

## Menús

### File

| Opción | Descripción |
| --- | --- |
| New | Crear un proyecto nuevo |
| Load | Abrir un proyecto existente |
| Save | Guardar el proyecto actual |
| Save As... | Guardar con otro nombre |
| Import MVR... | Cargar un `.mvr` |
| Export MVR... | Exportar la escena a MVR |
| Print Viewer 2D... | Exportar la vista 2D a PDF |
| Print Layout... | Exportar el layout activo a PDF |
| Print Table... | Imprimir o previsualizar la tabla activa |
| Export CSV... | Exportar datos de tabla a CSV |
| Close | Cerrar la aplicación |

### Edit

| Opción | Descripción |
| --- | --- |
| Undo / Redo | Deshacer o rehacer la última acción |
| Add fixture... | Insertar un fixture |
| Add truss... | Insertar un truss |
| Add scene object... | Insertar un objeto de escena |
| Delete | Eliminar la selección |
| Preferences... | Abrir Preferencias |

### View

| Opción | Descripción |
| --- | --- |
| Console | Mostrar/ocultar la consola |
| Fixtures | Mostrar/ocultar el panel de tablas |
| 3D Viewport | Mostrar/ocultar el visor 3D |
| 2D Viewport | Mostrar/ocultar el visor 2D |
| 2D Render Options | Mostrar/ocultar opciones 2D |
| Layers | Mostrar/ocultar capas |
| Layouts | Mostrar/ocultar layouts |
| Summary | Mostrar/ocultar resumen |
| Rigging | Mostrar/ocultar rigging |
| Layout Views | Cambiar entre vistas 3D, 2D y Modo layout |

### Tools

| Opción | Descripción |
| --- | --- |
| Download GDTF fixture... | Descargar fixtures desde GDTF‑Share |
| Edit dictionaries... | Editar diccionarios de riders |
| Create from text... | Importar riders desde texto o PDF |
| Export Fixture... | Exportar fixtures seleccionados |
| Export Truss... | Exportar trusses seleccionados |
| Export Scene Object... | Exportar objetos seleccionados |
| Auto patch | Asignar direcciones DMX automáticamente |
| Auto color | Asignar colores a capas y fixtures |
| Convert to Hoist | Convertir fixtures en hoists |

## Tablas y paneles

### Tablas de Fixtures / Trusses / Hoists / Objects

- Cada tabla muestra los elementos del proyecto actual.
- Las rotaciones están etiquetadas como **Roll (X)**, **Pitch (Y)** y **Yaw (Z)**
  y se aplican en orden Z/Y/X.
- Los menús contextuales permiten editar, reemplazar, exportar o gestionar direcciones.

### Panel de capas (Layers)

- Muestra las capas usadas por fixtures, trusses, hoists y objetos de escena.
- Activa/desactiva la visibilidad por capa desde **Visible**.
- Selecciona una capa para fijarla como **capa activa** al crear nuevos elementos.
- Haz clic derecho para asignar un color; los colores se usan en vistas 2D/3D.

### Panel de layouts

- Lista las páginas de layout (hojas imprimibles) del proyecto.
- Usa **Add**, **Rename** y **Delete** para gestionar páginas.
- Cambia la orientación (vertical/horizontal) desde el menú contextual.
- El orden de páginas sigue la lista; reordena donde sea posible arrastrando.

### Paneles Summary y Rigging

- **Summary** muestra conteos y estadísticas básicas por tipo de elemento.
- **Rigging** suma pesos y resalta datos de peso faltantes.

### Panel 2D Render Options

Controla el modo de renderizado 2D, la cuadrícula y las etiquetas.

### Panel de consola

Muestra mensajes de estado y permite comandos de edición rápida.

### Atajos de edición en tablas

- **Aplicar un valor a toda la selección:** escribe un número (o texto).
- **Interpolar valores:** introduce inicio y fin (`1 12` o `1 thru 12`).
- **Valores secuenciales:** termina con un espacio (`100␠`) o con `t` (`100 t`).
- **Separadores de rango:** `t` o `thru` con o sin espacios (`1t10`, `1 t 10`).
- **Edición relativa:** usa `++` / `--` (por ejemplo `++0.5`, `--15`).

## Modo de layout e impresión

### Resumen del modo de layout

El modo de layout permite crear páginas imprimibles con vistas 2D, leyendas,
tablas de eventos, cuadros de texto e imágenes. Las barras **Layout Views** y
**Layout** ofrecen iconos para estas acciones y replican los comandos de los menús.
La barra **File** incluye accesos a New/Load/Save/Save As e importación/exportación MVR.

### Herramientas de layout (toolbar y menú)

- **Añadir vista 2D**: inserta una vista en planta.
- **Añadir leyenda**: agrega una leyenda de fixtures.
- **Añadir tabla de evento**: agrega una tabla de eventos.
- **Añadir texto**: inserta un cuadro de texto.
- **Añadir imagen**: inserta una imagen.

Organiza los elementos arrastrándolos y usa el menú contextual para editar,
eliminar y cambiar el orden en profundidad (traer al frente / enviar atrás).

### Impresión y exportación (menú File)

- **Print Viewer 2D...** exporta la vista 2D a PDF. Permite elegir tamaño de página
  (A3/A4), orientación, cuadrícula y nivel de detalle.
- **Print Layout...** exporta el layout activo a PDF. Permite elegir tamaño de página
  y detalle 2D; la orientación se toma de la página.
- **Print Table...** muestra una vista previa para imprimir. Puedes elegir columnas
  antes de imprimir (el PDF depende del diálogo de impresión del sistema).
- **Export CSV...** exporta las columnas seleccionadas a un archivo `.csv`.

## Herramientas (Tools)

- **Download GDTF fixture...**: busca y descarga fixtures desde **GDTF‑Share**.
  El inicio de sesión guarda credenciales localmente de forma encriptada.
- **Edit dictionaries...**: edita los diccionarios usados en el importador de riders.
- **Create from text...**: importa riders desde texto o PDF. Usa el diccionario para
  resolver nombres de fixtures y mapearlos a archivos GDTF.
- **Export Fixture / Export Truss / Export Scene Object...**: exporta elementos seleccionados.
- **Auto patch**: asigna direcciones DMX automáticamente.
- **Auto color**: asigna colores por capa y colores de fixture por tipo; las capas de
  truss se vuelven gris claro y se respetan colores ya definidos.
- **Convert to Hoist**: convierte fixtures a hoists manteniendo nombre y posición.

## Comandos de consola

La consola permite seleccionar elementos y aplicar transformaciones con precisión.

### Selección

- `f <ids>`: selecciona fixtures. Mantiene la selección actual y admite `+`/`-`.
- `t <ids>`: selecciona trusses (reemplaza la selección de trusses).
- `clear`: limpia la selección.

Rangos e incrementos soportados:

- `1 thru 10`, `1 t 10`, `1t10`
- `+ 5` o `- 3` dentro de un comando de selección

Ejemplos:

```
f 1 4              # selecciona fixtures 1 a 4
f + 6 8            # añade fixtures 6 a 8
f - 2              # elimina el fixture 2
t 1 3              # selecciona trusses 1 a 3
clear              # limpia selección
```

### Posición y rotación

- `pos x 1.5` fija X a 1,5 m.
- `pos 0 0 5` fija X/Y/Z directamente.
- `pos 1,2,3` usa valores separados por comas para X/Y/Z.
- `x ++0.5` mueve solo el eje X en modo relativo.
- `rot z 90` fija yaw a 90°.
- `rot ++45` aplica una rotación relativa en todos los ejes.

Puedes introducir dos valores para interpolar en la selección (por ejemplo
`pos x 0 10`). Las posiciones están en metros y las rotaciones en grados.

## Preferencias

Abre **Edit → Preferences...** para configurar el importador de riders:

- Auto‑patch tras la importación.
- Creación automática de capas por **posición** o por **tipo de fixture**.
- Alturas, posiciones y márgenes por defecto (LX1–LX6) en metros.

## Formatos compatibles

- **MVR 1.6**: importación y exportación de fixtures, trusses, hoists y objetos.
- **GDTF**: fixtures y modelos embebidos.
- **3DS / GLB**: modelos 3D usados por MVR/GDTF.

## Resolución de problemas

- Si no se muestra el visor 3D:
  - Comprueba que OpenGL está instalado.
  - Asegúrate de que el MVR contiene geometría válida y referencias GDTF.
- Si no aparecen objetos en las tablas:
  - Verifica que el MVR define correctamente los objetos.
  - Revisa la consola por advertencias o errores.

## Documentación adicional

- Especificación MVR: `docs/mvr-spec.md`
- Especificación GDTF: `docs/gdtf-spec.md`
- Licencia: `LICENSE.txt` (GPL v3)

</details>
