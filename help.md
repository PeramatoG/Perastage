<!-- LANG:en -->
# Perastage Help

## 1) What Perastage is

Perastage is a desktop tool to inspect and prepare stage scenes from `.mvr` and `.pstg` files with 3D, 2D, table, and layout workflows.

## 2) Opening files and projects

- **File > New** creates a new `.pstg` project.
- **File > Load** opens an existing `.pstg` project.
- **File > Save** and **File > Save As...** store project changes.
- **File > Import MVR...** imports an `.mvr` scene into the current workflow.

## 3) Main interface overview

Main production panels:

- **3D Viewer** for perspective review and selection.
- **2D Viewer** for top/layout-oriented navigation.
- **Fixtures / Trusses / Hoists / Objects** tables for scene data editing.
- **Layers** for visibility and active-layer control.
- **Layouts** for printable page composition.
- **Console** for command-based selection and transform workflows.

Use **View** to show or hide panels.

## 4) 3D view controls

Keyboard:

- Arrow keys: orbit
- Shift + arrow keys: pan
- Alt + arrow keys: zoom
- Numpad 1 / 3 / 7: Front / Right / Top views
- Numpad 5: reset camera orientation
- Z: frame scene

Mouse:

- Left drag: orbit
- Shift + left drag or middle drag: pan
- Mouse wheel: zoom
- Left click: select object under cursor
- Shift/Ctrl + left click: toggle selection
- Ctrl + left drag: rectangle selection

## 5) 2D/layout view controls

### 2D Viewer

Keyboard:

- Arrow keys: pan
- Alt + arrow keys: zoom

Mouse:

- Left drag on empty space: pan
- Mouse wheel: zoom
- Left click: select object
- Shift/Ctrl + left click: toggle selection
- Ctrl + left drag: rectangle selection
- Left drag on selected items: move selection

### Layout View

Keyboard:

- Delete: delete selected element
- Z: fit layout view

Mouse:

- Left drag frame handle: resize element
- Left drag frame: move element
- Left drag empty space: pan
- Mouse wheel: zoom
- Double click frame: edit element
- Right click frame: context actions

## 6) Tables and scene data panels

- **Fixtures**: fixture patch, type, positioning, and related metadata.
- **Trusses**: truss units and structural model data.
- **Hoists**: load-related rigging elements.
- **Objects**: generic scene objects, including primitive geometry objects.
- **Summary** and **Rigging** panels provide quick overview and aggregated checks.

## 7) Adding and editing objects

You can add scene objects from the GUI and later edit them through scene selection and table workflows.

New objects are assigned to the active layer.

## 8) `Add basic geometry`

Use **Edit > Add basic geometry** and choose one of:

- **Sphere...**
  - Radius
  - Quantity
- **Cube...**
  - Length
  - Height
  - Width
  - Quantity
- **Cylinder...**
  - Top radius
  - Bottom radius
  - Height
  - Quantity

Dimension inputs follow the current distance preference from **Edit > Preferences... > Units**:

- Metric mode: plain values are interpreted as meters.
- Imperial mode: plain values are interpreted as feet.
- Explicit unit suffixes (`m`, `ft`, `in`) are accepted in both modes.

## 9) `Create from text`

Use **Tools > Create from text...** to create fixtures, trusses, and scene objects from rider-style text.

### Quick behavior

- **Apply filter** normalizes the current text.
- **Create** parses the text currently visible in the editor and creates scene data.
- Quantity, hang, and optional coordinate/margin tokens are respected when parsed.

## 10) Layouts, printing, and export

- Use **Layouts** to build printable pages with views, legends, tables, text, and images.
- Export project scene data with MVR workflows.
- Some print/debug utilities can be build-dependent.

## 11) Preferences

Open **Edit > Preferences...**.

Important user-facing options include unit systems:

- **Distance system:** Metric (`m`) or Imperial (`ft`)
- **Weight system:** Metric (`kg`) or Imperial (`lb`)

Internal canonical values stay in `mm` (distance) and `kg` (weight).

## 12) Keyboard shortcuts

### Global

- Ctrl+N: New project
- Ctrl+L: Load project
- Ctrl+S: Save project
- Ctrl+Q: Close application
- Ctrl+Z / Ctrl+Y: Undo / Redo
- Del: Delete selection
- F1: Open Help
- F: Focus CLI with prefill token
- 1 / 2 / 3 / 4: switch Fixtures / Trusses / Hoists / Objects tabs

### Console input

- Esc: exit prompt and restore app shortcuts
- Up / Down: history
- Home: move to start of editable input

## 13) Text commands and keywords

### Fixtures

Basic fixture pattern:

```text
N Fixture Type
```

Example:

```text
12 Spot 1500
```

### Trusses

Accepted truss line forms include `truss` with optional length and target hang:

```text
1 truss 40X40 12m para LX1
1 truss 40X40 for LX2
```

### Pipes / varas

Accepted keywords: `pipe`, `pipes`, `vara`, `varas`.

Examples:

```text
1 pipe 12m para LX1
2 varas para LX2
```

Current behavior:

- Pipe/vara lines create scene objects represented as cylinder primitives.
- If length is omitted, default length is `14 m`.
- Generated names follow `PIPE <hang>` (example: `PIPE LX1`).
- Pipe objects are separate from truss objects and appear in **Objects**.

### Advanced primitive tokens (reference)

Internal primitive identifiers used in advanced data flows include:

- `primitive:sphere`
- `primitive:cube`
- `primitive:cylinder;top=...;bottom=...;height=...`

For the full parser contract, see `docs/text_to_scene_rules.md`.

## 14) Troubleshooting / common questions

- If panels are missing, re-enable them from **View**.
- If parser results are unexpected, run **Apply filter** first and verify hang/quantity lines.
- If fixture profiles are missing, verify local GDTF resources and optional login settings.

---

<!-- LANG:es -->
# Ayuda de Perastage

## 1) Qué es Perastage

Perastage es una herramienta de escritorio para inspeccionar y preparar escenas desde archivos `.mvr` y proyectos `.pstg`, con flujos 3D, 2D, tablas y layouts.

## 2) Abrir archivos y proyectos

- **File > New** crea un proyecto `.pstg`.
- **File > Load** abre un proyecto `.pstg`.
- **File > Save** y **File > Save As...** guardan cambios.
- **File > Import MVR...** importa una escena `.mvr`.

## 3) Vista general de la interfaz

Paneles principales:

- **3D Viewer**
- **2D Viewer**
- Tablas **Fixtures / Trusses / Hoists / Objects**
- **Layers**
- **Layouts**
- **Console**

Usa **View** para mostrar u ocultar paneles.

## 4) Controles de vista 3D

Teclado:

- Arrow keys: orbitar
- Shift + arrow keys: paneo
- Alt + arrow keys: zoom
- Numpad 1 / 3 / 7: Front / Right / Top
- Numpad 5: reset de orientación
- Z: encuadrar escena

Ratón:

- Left drag: orbitar
- Shift + left drag o middle drag: paneo
- Mouse wheel: zoom
- Left click: seleccionar
- Shift/Ctrl + left click: alternar selección
- Ctrl + left drag: selección por rectángulo

## 5) Controles de vista 2D/layout

### 2D Viewer

- Arrow keys: paneo
- Alt + arrow keys: zoom
- Left drag en vacío: paneo
- Mouse wheel: zoom
- Left click: selección

### Layout View

- Delete: borrar elemento seleccionado
- Z: ajustar vista
- Left drag handle/frame: redimensionar o mover
- Right click frame: menú contextual

## 6) Tablas y paneles de datos

- **Fixtures**, **Trusses**, **Hoists**, **Objects** para edición de datos de escena.
- **Summary** y **Rigging** para revisión agregada.

## 7) Añadir y editar objetos

Puedes crear objetos desde la GUI y editarlos después con la selección y tablas. Los objetos nuevos se añaden a la capa activa.

## 8) `Add basic geometry`

Usa **Edit > Add basic geometry**:

- **Sphere...**: Radius, Quantity
- **Cube...**: Length, Height, Width, Quantity
- **Cylinder...**: Top radius, Bottom radius, Height, Quantity

Las dimensiones usan la preferencia activa de distancia en **Edit > Preferences... > Units**.

## 9) `Create from text`

Usa **Tools > Create from text...** para crear fixtures, trusses y objetos desde texto tipo rider.

- **Apply filter** normaliza el texto.
- **Create** parsea el texto visible y crea datos.

## 10) Layouts, impresión y export

- **Layouts** permite preparar páginas imprimibles.
- Exporta/importa escenas con flujos MVR.

## 11) Preferencias

En **Edit > Preferences...** puedes configurar unidades:

- **Distance system:** Metric (`m`) o Imperial (`ft`)
- **Weight system:** Metric (`kg`) o Imperial (`lb`)

## 12) Atajos de teclado

- Ctrl+N, Ctrl+L, Ctrl+S, Ctrl+Q
- Ctrl+Z / Ctrl+Y
- Del, F1, F
- 1 / 2 / 3 / 4

## 13) Comandos y keywords de texto

Keywords válidas para pipes/varas: `pipe`, `pipes`, `vara`, `varas`.

Ejemplos:

```text
1 pipe 12m para LX1
2 varas para LX2
```

- Si no indicas longitud, se usa `14 m`.
- El nombre generado sigue `PIPE <hang>`.
- Se crean como cilindros en **Objects**.

## 14) Solución de problemas

- Si falta un panel, reactívalo desde **View**.
- Si el parser no da el resultado esperado, usa **Apply filter** y revisa cantidades/hangs.
