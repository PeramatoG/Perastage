<!-- LANG:en -->
# Perastage Help

## Quick Start

1. Launch the application.
2. Use **File > Import MVR...** to load an `.mvr` file.
3. Use the table tabs (**Fixtures**, **Trusses**, **Hoists**, **Objects**) to inspect data.
4. Toggle panels via the **View** menu if any pane is hidden.

## Project Files

Perastage projects (`.psproj`) store the scene, layouts, and user settings.

**File > New / Load / Save / Save As...**

- **New** creates a blank project.
- **Load** opens an existing `.psproj` file.
- **Save** stores changes in the current project file.
- **Save As...** writes the project under a new name or location.

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
- **Layouts** manages printable layout pages.
- **Summary** shows counts and statistics per object type.
- **Rigging** aggregates total weights and highlights missing data.
- **2D Render Options** controls grid/labels for the 2D viewport.

<!-- LANG:es -->
# Ayuda de Perastage

## Inicio rápido

1. Abre la aplicación.
2. Usa **Archivo > Importar MVR...** para cargar un `.mvr`.
3. Usa las pestañas (**Fixtures**, **Trusses**, **Hoists**, **Objects**) para revisar los datos.
4. Activa/desactiva paneles desde el menú **View** si alguno está oculto.

## Archivos de proyecto

Los proyectos de Perastage (`.psproj`) guardan la escena, los layouts y la configuración del usuario.

**Archivo > Nuevo / Cargar / Guardar / Guardar como...**

- **Nuevo** crea un proyecto en blanco.
- **Cargar** abre un `.psproj` existente.
- **Guardar** guarda los cambios en el proyecto actual.
- **Guardar como...** guarda el proyecto con otro nombre o en otra ubicación.

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
- **Layouts** gestiona las páginas de layout.
- **Summary** muestra conteos y estadísticas por tipo.
- **Rigging** agrega pesos totales y marca datos faltantes.
- **2D Render Options** controla la cuadrícula y etiquetas en el visor 2D.
