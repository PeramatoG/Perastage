<!-- LANG:en -->
# Perastage Help

## Quick Start

1. Launch the application.
2. Use **File > Import MVR...** to load an `.mvr` file.
3. Use the table tabs (**Fixtures**, **Trusses**, **Hoists**, **Objects**) to inspect data.
4. Toggle panes via the **View** menu if any pane is hidden (Console, Layers, Layouts, Summary, Rigging, 2D Viewer, 3D Viewer, and 2D Render Options).

## Project Files

Perastage projects (`.pstg`) store the scene, layouts, and user settings.

**File > New / Load / Save / Save As...**

- **New** creates a new project. If default layout templates are available in
  `library/default_layouts/`, they are loaded; otherwise Perastage keeps the
  built-in blank layout.
- **Load** opens an existing `.pstg` file.
- **Save** stores changes in the current project file.
- **Save As...** writes the project under a new name or location.

## Units (Metric/Imperial)

Set unit systems in **Edit > Preferences > Units**:

- **Distance system:** Metric (`m`) or Imperial (`ft`)
- **Weight system:** Metric (`kg`) or Imperial (`lb`)

Behavior:

- Internal canonical values remain in **mm** (distance) and **kg** (weight).
- The unit preference changes **display labels** and **plain-number input interpretation**.
- Inputs with explicit suffixes (`m`, `ft`, `in`, `kg`, `lb`) are accepted regardless of active system.

Examples:

- Metric distance active: entering `6.2` means `6.2 m`.
- Imperial distance active: entering `6.2` means `6.2 ft`.
- Entering `5' 6"` always parses as feet/inches distance.
- Metric weight active: entering `120` means `120 kg`; imperial means `120 lb`.

Formatting precision depends on UI context (table/label/inspector). See [`docs/ui_unit_systems.md`](docs/ui_unit_systems.md) for complete parsing and rounding rules.

## Tools > Create from text... (quick guide)

Use **Tools > Create from text...** to paste rider text, normalize it, and create scene objects.

Minimal examples:

- Coordinate override in a hang token: `LX1 (0, -1, 9)`
  - Sets explicit `x, y, z` for that hang/truss target.
- Hang margin override: `LX1 [0.8]`
  - Overrides the default distribution margin for that hang.
- Override precedence when both are present:
  - Truss-line overrides take precedence over hang-header overrides for the values provided in the truss line (both for `(x, y, z)` and `[margin]`).

**Apply filter** vs **Create**:

- **Apply filter** rewrites the input into normalized text (preview/edit step).
- **Create** parses the current text in the editor and generates fixtures/trusses/objects.
- If you use **Apply filter** first, then **Create**, creation uses the filtered text currently shown in the dialog.
- Coordinate/margin tokens such as `(0, -1, 9)` and `[0.8]` are preserved by the filter step, so their effect remains when creating.

For the full technical contract (all parsing and placement rules), see [`docs/text_to_scene_rules.md`](docs/text_to_scene_rules.md).

## Build-dependent tools

Some tools are intentionally available only in **Debug** builds:

- **File > Print Viewer 2D...**
- **Tools > Generate fixture symbols**

Release builds keep these entries hidden to reduce risk in production workflows.

## File associations by operating system

- **Windows installer (official: Inno Setup):** `packaging/windows/Perastage.iss` registers ProgID/icon/open-command entries for `.pstg`, plus an optional installer task (`assoc_mvr`) to associate `.mvr` for double-click import flow.
- **Linux install:** deploys a desktop entry plus MIME XML declarations for `*.mvr` and `*.pstg`, then refreshes MIME/desktop caches when the relevant tools are available.
- **macOS bundle:** exports `CFBundleDocumentTypes` for `.mvr` so files can be opened from Finder.

## Console Commands (complete)

The console works on the current selection. If fixtures are selected, position
and rotation commands apply to fixtures; otherwise they apply to trusses.

### Selection

| Command | Description |
| --- | --- |
| `clear` | Clears all selections (fixtures, trusses, scene objects). |
| `f ...` | Select fixtures by ID. |
| `t ...` | Select trusses by unit number (clears current truss selection first). |

Selection syntax supports:

- Single IDs: `f 12`
- Ranges: `f 1-5`, `f 1 thru 5`, `f 1 t 5`
- Add/remove: `f + 10 - 3`
- Mixed tokens: `f 1 3 5 7-9`

### Position and rotation

| Command | Description |
| --- | --- |
| `pos x <values>` | Set X positions for the selection. |
| `pos y <values>` | Set Y positions for the selection. |
| `pos z <values>` | Set Z positions for the selection. |
| `pos <x>,<y>,<z>` | Set X/Y/Z in one command. |
| `x <values>` | Shortcut for `pos x`. |
| `y <values>` | Shortcut for `pos y`. |
| `z <values>` | Shortcut for `pos z`. |
| `rot x <values>` | Set rotation around X (roll). |
| `rot y <values>` | Set rotation around Y (pitch). |
| `rot z <values>` | Set rotation around Z (yaw). |

Notes:

- Provide **one value** to apply it to all selected items.
- Provide **two values** to linearly distribute from start to end across the selection.
- Use `++` / `--` to apply relative offsets (example: `pos x ++ 1.5`, `rot z -- 10`).
- You can also type a comma-separated triplet like `1, 2, 3` as a shortcut for `pos`.

## Keyboard Shortcuts (complete)

### Global

| Shortcut | Action |
| --- | --- |
| Ctrl+N | New project |
| Ctrl+L | Load project |
| Ctrl+S | Save project |
| Ctrl+Q | Close application |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Del | Delete selection |
| F1 | Open help |
| F | Focus CLI and prefill `fixture ` (outside editable widgets) |
| 1 / 2 / 3 / 4 | Switch to Fixtures / Trusses / Hoists / Objects |

### Console input

| Shortcut | Action |
| --- | --- |
| Esc | Exit the prompt and re-enable app shortcuts |
| Up / Down | Navigate command history |
| Home | Move to the start of input (after the prompt) |
| Left / Backspace | Cannot move before the prompt |

### 3D Viewer (keyboard)

| Shortcut | Action |
| --- | --- |
| Arrow keys | Orbit camera |
| Shift + arrow keys | Pan camera |
| Alt + arrow keys | Zoom in/out |
| Numpad 1 / 3 / 7 | Front / Right / Top views |
| Numpad 5 | Reset camera orientation |
| Z | Frame scene (fit all objects in view) |

### 2D Viewer (keyboard)

| Shortcut | Action |
| --- | --- |
| Arrow keys | Pan view |
| Alt + arrow keys | Zoom in/out |

### Layout View (keyboard)

| Shortcut | Action |
| --- | --- |
| Delete | Delete selected layout element |
| Z | Reset layout view to fit |

## Mouse Shortcuts (complete)

### 3D Viewer

| Action | Result |
| --- | --- |
| Left drag | Orbit camera |
| Shift + left drag or middle drag | Pan camera |
| Mouse wheel | Zoom in/out |
| Left click | Select fixture/truss/object under the cursor |
| Shift/Ctrl + left click | Toggle selection |
| Ctrl + left drag | Rectangle select |
| Double click a fixture label | Open fixture patch dialog |

## 3D Viewer > Render style

Open it from the **3D Viewer** context menu: right click inside the 3D viewport, then use **Render style**.

Current render styles:

- **Standard**
- **Sketch mode**
- **Textured**
- **Wireframe**
- **White**
- **By device type**
- **By layer**
- **By universe**

Recommended use by style:

- **Standard**: balanced everyday view for quick scene reading and navigation.
- **Sketch mode**: flat white model with line emphasis; useful for geometry readability in dense rigs.
- **Textured**: material-aware look; useful for visual checks of textured assets and scenic context.
- **Wireframe**: edge-only visualization; useful for technical debugging, overlap inspection, and internal structure checks.
- **White**: neutral shaded white view; useful for lighting-independent shape review and print-friendly captures.
- **By device type**: color grouping by fixture/device category; useful for fast visual classification and patch sanity checks.
- **By layer**: color grouping by layer; useful for validating layer organization and layer-based workflows.
- **By universe**: color grouping by DMX universe; useful for patch/debug tasks related to universe distribution.

### 2D Viewer

| Action | Result |
| --- | --- |
| Left drag (empty space) | Pan view |
| Mouse wheel | Zoom in/out |
| Left click | Select fixture/truss/object under the cursor |
| Shift/Ctrl + left click | Toggle selection |
| Ctrl + left drag | Rectangle select |
| Left drag on selected items | Move selection (axis locks to initial drag direction) |

### Layout View

| Action | Result |
| --- | --- |
| Left drag on a frame handle | Resize element |
| Left drag on a frame | Move element |
| Left drag on empty space | Pan the layout view |
| Mouse wheel | Zoom in/out |
| Double click a frame | Edit that element (view/table/text/image) |
| Right click a frame | Open context menu (edit/delete/stacking) |

## Panels

- **Console** shows status messages and accepts console commands.
- **Layers** shows layer list, visibility, and active layer for new items.
- **Layouts** manages printable layout pages.
- **Summary** shows counts and statistics per object type.
- **Rigging** aggregates total weights and highlights missing data.
- **2D Render Options** controls grid/labels for the 2D viewport.

<!-- LANG:es -->
# Ayuda de Perastage

## Inicio rápido

1. Abre la aplicación.
2. Usa **File > Import MVR...** para cargar un `.mvr`.
3. Usa las pestañas (**Fixtures**, **Trusses**, **Hoists**, **Objects**) para revisar los datos.
4. Activa/desactiva paneles desde el menú **View** si alguno está oculto.

## Archivos de proyecto

Los proyectos de Perastage (`.pstg`) guardan la escena, los layouts y la configuración del usuario.

**File > New / Load / Save / Save As...**

- **Nuevo** crea un proyecto en blanco.
- **Cargar** abre un `.pstg` existente.
- **Guardar** guarda los cambios en el proyecto actual.
- **Guardar como...** guarda el proyecto con otro nombre o en otra ubicación.

## Unidades (Métrico/Imperial)

Configura las unidades en **Edit > Preferences > Units**:

- **Distance system:** Metric (`m`) o Imperial (`ft`)
- **Weight system:** Metric (`kg`) o Imperial (`lb`)

Comportamiento:

- Internamente los valores canónicos siguen en **mm** (distancia) y **kg** (peso).
- La preferencia cambia las **etiquetas de visualización** y cómo se interpreta una entrada numérica sin sufijo.
- Entradas con sufijo explícito (`m`, `ft`, `in`, `kg`, `lb`) funcionan en cualquier sistema activo.

Ejemplos:

- Distancia en metric: `6.2` se interpreta como `6.2 m`.
- Distancia en imperial: `6.2` se interpreta como `6.2 ft`.
- `5' 6"` siempre se interpreta como pies/pulgadas.
- Peso en metric: `120` se interpreta como `120 kg`; en imperial como `120 lb`.

La precisión de redondeo/formato depende del contexto (tabla/label/inspector). Consulta [`docs/ui_unit_systems.md`](docs/ui_unit_systems.md) para reglas completas de parseo y redondeo.

## Tools > Create from text... (guía rápida)

Usa **Tools > Create from text...** para pegar texto de rider, normalizarlo y crear objetos en la escena.

Ejemplos mínimos:

- Override de coordenadas en el token de hang: `LX1 (0, -1, 9)`
  - Define `x, y, z` explícitos para ese hang/truss objetivo.
- Override de margen por hang: `LX1 [0.8]`
  - Sobrescribe el margen por defecto de distribución para ese hang.
- Precedencia de overrides cuando ambos existen:
  - Los overrides en la línea de truss tienen prioridad sobre los del header del hang para los valores definidos en la línea de truss (tanto `(x, y, z)` como `[margen]`).

Relación entre **Apply filter** y **Create**:

- **Apply filter** reescribe la entrada a texto normalizado (paso de vista previa/edición).
- **Create** parsea el texto actual del editor y genera fixtures/trusses/objetos.
- Si primero usas **Apply filter** y luego **Create**, la creación usa el texto filtrado que quedó visible en el diálogo.
- Tokens de coordenadas/margen como `(0, -1, 9)` y `[0.8]` se preservan durante el filtrado, así que su efecto se mantiene al crear.

Para el contrato técnico completo (todas las reglas de parseo y placement), consulta [`docs/text_to_scene_rules.md`](docs/text_to_scene_rules.md).

## Comandos de consola (completo)

La consola trabaja sobre la selección actual. Si hay fixtures seleccionados,
los comandos de posición/rotación se aplican a fixtures; si no, se aplican a trusses.

### Selección

| Comando | Descripción |
| --- | --- |
| `clear` | Borra todas las selecciones (fixtures, trusses, scene objects). |
| `f ...` | Selecciona fixtures por ID. |
| `t ...` | Selecciona trusses por número de unidad (limpia la selección previa de trusses). |

La sintaxis de selección admite:

- IDs sueltos: `f 12`
- Rangos: `f 1-5`, `f 1 thru 5`, `f 1 t 5`
- Añadir/quitar: `f + 10 - 3`
- Combinado: `f 1 3 5 7-9`

### Posición y rotación

| Comando | Descripción |
| --- | --- |
| `pos x <valores>` | Asigna posiciones en X a la selección. |
| `pos y <valores>` | Asigna posiciones en Y a la selección. |
| `pos z <valores>` | Asigna posiciones en Z a la selección. |
| `pos <x>,<y>,<z>` | Asigna X/Y/Z en un solo comando. |
| `x <valores>` | Atajo de `pos x`. |
| `y <valores>` | Atajo de `pos y`. |
| `z <valores>` | Atajo de `pos z`. |
| `rot x <valores>` | Rota alrededor de X (roll). |
| `rot y <valores>` | Rota alrededor de Y (pitch). |
| `rot z <valores>` | Rota alrededor de Z (yaw). |

Notas:

- Un **solo valor** se aplica a toda la selección.
- **Dos valores** se distribuyen linealmente de inicio a fin.
- Usa `++` / `--` para aplicar incrementos relativos (ej.: `pos x ++ 1.5`, `rot z -- 10`).
- También puedes escribir un triplete separado por comas como `1, 2, 3`.

## Atajos de teclado (completo)

### Globales

| Atajo | Acción |
| --- | --- |
| Ctrl+N | Nuevo proyecto |
| Ctrl+L | Cargar proyecto |
| Ctrl+S | Guardar proyecto |
| Ctrl+Q | Cerrar aplicación |
| Ctrl+Z / Ctrl+Y | Deshacer / Rehacer |
| Del | Borrar selección |
| F1 | Abrir ayuda |
| F | Enfocar CLI y rellenar `fixture ` (fuera de widgets editables) |
| 1 / 2 / 3 / 4 | Ir a Fixtures / Trusses / Hoists / Objects |

### Entrada de consola

| Atajo | Acción |
| --- | --- |
| Esc | Salir del prompt y reactivar atajos de la app |
| Arriba / Abajo | Recorrer el historial |
| Home | Ir al inicio del input (después del prompt) |
| Izquierda / Retroceso | No permite pasar antes del prompt |

### Visor 3D (teclado)

| Atajo | Acción |
| --- | --- |
| Flechas | Orbitar cámara |
| Shift + flechas | Desplazar cámara |
| Alt + flechas | Zoom +/- |
| Numpad 1 / 3 / 7 | Vista frontal / derecha / superior |
| Numpad 5 | Resetear cámara |
| Z | Encuadrar escena (ajustar todo a vista) |

### Visor 2D (teclado)

| Atajo | Acción |
| --- | --- |
| Flechas | Desplazar vista |
| Alt + flechas | Zoom +/- |

### Vista Layout (teclado)

| Atajo | Acción |
| --- | --- |
| Delete | Borrar elemento seleccionado |
| Z | Ajustar vista al layout |

## Atajos de ratón (completo)

### Visor 3D

| Acción | Resultado |
| --- | --- |
| Arrastrar con botón izquierdo | Orbitar cámara |
| Shift + arrastrar izquierdo o botón central | Desplazar cámara |
| Rueda | Zoom +/- |
| Click izquierdo | Seleccionar fixture/truss/objeto bajo el cursor |
| Shift/Ctrl + click izquierdo | Alternar selección |
| Ctrl + arrastrar izquierdo | Selección por rectángulo |
| Doble click en etiqueta de fixture | Abrir patch de fixture |

## 3D Viewer > Render style

Ábrelo desde el menú contextual del **3D Viewer**: click derecho dentro del visor 3D y luego **Render style**.

Estilos de render actuales:

- **Standard**
- **Sketch mode**
- **Textured**
- **Wireframe**
- **White**
- **By device type**
- **By layer**
- **By universe**

Uso recomendado por estilo:

- **Standard**: vista equilibrada para lectura visual rápida y navegación diaria.
- **Sketch mode**: modelo blanco plano con énfasis en líneas; útil para leer geometría en rigs densos.
- **Textured**: visualización con materiales; útil para revisar assets texturizados y contexto escénico.
- **Wireframe**: visualización solo de aristas; útil para depuración técnica, revisar solapes y estructura interna.
- **White**: sombreado blanco neutro; útil para revisar formas sin depender de color/iluminación y para capturas limpias.
- **By device type**: agrupación por color según tipo de fixture/dispositivo; útil para clasificación visual rápida y checks de patch.
- **By layer**: agrupación por color por capa; útil para validar organización por capas y flujos layer-based.
- **By universe**: agrupación por color por universo DMX; útil para tareas de patch y depuración de distribución por universos.

### Visor 2D

| Acción | Resultado |
| --- | --- |
| Arrastrar con botón izquierdo (espacio vacío) | Desplazar vista |
| Rueda | Zoom +/- |
| Click izquierdo | Seleccionar fixture/truss/objeto bajo el cursor |
| Shift/Ctrl + click izquierdo | Alternar selección |
| Ctrl + arrastrar izquierdo | Selección por rectángulo |
| Arrastrar selección | Mover selección (se bloquea el eje inicial) |

### Vista Layout

| Acción | Resultado |
| --- | --- |
| Arrastrar un tirador de marco | Redimensionar elemento |
| Arrastrar un marco | Mover elemento |
| Arrastrar en vacío | Panear la vista |
| Rueda | Zoom +/- |
| Doble click en un marco | Editar elemento (vista/tabla/texto/imagen) |
| Click derecho en un marco | Menú contextual (editar/borrar/orden) |

## Paneles

- **Console** muestra mensajes de estado y acepta comandos.
- **Layers** muestra capas, visibilidad y la capa activa para nuevos elementos.
- **Layouts** gestiona las páginas de layout.
- **Summary** muestra conteos y estadísticas por tipo.
- **Rigging** agrega pesos totales y marca datos faltantes.
- **2D Render Options** controla la cuadrícula y etiquetas en el visor 2D.
