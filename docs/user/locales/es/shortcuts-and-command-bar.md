# Atajos y barra de comandos

Esta página reúne los atajos de teclado, ratón y barra de comandos más útiles de la ayuda integrada para trabajar con rapidez sin salir del visor de documentación.

## Edición de escena

- `Ctrl+X`: corta las luminarias, trusses, motores y objetos de escena seleccionados.
- `Ctrl+C`: copia los elementos de escena seleccionados.
- `Ctrl+V`: pega los elementos copiados en el proyecto actual.

Estos comandos admiten selecciones mixtas y también están disponibles en el menú **Edit**. Al editar un campo de texto o una celda, las mismas teclas conservan su función habitual de edición de texto.

## Atajos generales

| Atajo | Acción |
| --- | --- |
| `Ctrl+N` | Nuevo proyecto |
| `Ctrl+L` | Cargar proyecto |
| `Ctrl+S` | Guardar proyecto |
| `Ctrl+Q` | Cerrar aplicación |
| `Ctrl+Z / Ctrl+Y` | Deshacer / rehacer |
| `Del` | Eliminar selección |
| `Ctrl+G` | Agrupar la selección activa de varias tablas |
| `Ctrl+U` | Sacar los objetos seleccionados de su grupo inmediato |
| `F1` | Abrir ayuda |
| `Ctrl+Alt+D` / `Ctrl+Shift+D` | Distribuir luminarias sobre truss / entre puntos |
| `P` / `R` | Activar Console y rellenar `pos ` / `rot ` fuera de controles editables |
| `F` | Activar Console y rellenar `fixture ` (fuera de controles editables) |
| `1 / 2 / 3 / 4` | Cambiar a Fixtures / Trusses / Hoists / Objects |

## Atajos de los visores

| Atajo | Acción |
| --- | --- |
| `Z` | Ajustar la vista (visor 2D/3D activo) |
| `NumPad1` | Vista frontal |
| `NumPad3` | Vista lateral/derecha |
| `NumPad7` | Vista superior |
| `NumPad5` | Restablecer orientación de la cámara 3D |
| `Arrow keys` (3D) | Orbitar la cámara |
| `Shift + Arrow keys` (3D) | Desplazar la cámara |
| `Alt + Arrow keys` (2D/3D) | Acercar/alejar |
| `Arrow keys` (2D) | Desplazar la vista |

## Atajos de Layout View

| Atajo | Acción |
| --- | --- |
| `Delete` | Eliminar el elemento de layout seleccionado |
| `Z` | Ajustar la vista del layout |

## Atajos de entrada de Console

| Atajo | Acción |
| --- | --- |
| `Esc` | Salir de la entrada y reactivar los atajos de la aplicación |
| `Up / Down` | Recorrer el historial de comandos |
| `Home` | Ir al inicio de la entrada (tras el indicador) |
| `Left / Backspace` | No permite moverse antes del indicador |

## Atajos de ratón (visor 3D)

| Acción | Resultado |
| --- | --- |
| Arrastre con botón izquierdo | Orbitar la cámara |
| Shift + arrastre izquierdo o arrastre central | Desplazar la cámara |
| Rueda del ratón | Acercar/alejar |
| Clic izquierdo | Seleccionar luminaria/truss/objeto bajo el cursor |
| Shift/Ctrl + clic izquierdo | Alternar selección |
| Ctrl + arrastre izquierdo | Seleccionar mediante rectángulo |
| Doble clic en la etiqueta de luminaria | Abrir el diálogo de patch de luminaria |

## Uso rápido de la barra de comandos

1. Activa la barra de comandos.
2. Fuera de controles editables, utiliza `P`, `R` o `F` para activar y rellenar Console.
3. Edita el texto del comando.
4. Ejecuta y comprueba el resultado en tablas y visores.
5. Reutiliza el historial para cambios repetitivos.

## CLI experimental

A partir de la versión 1.7.0, Perastage incluye una interfaz experimental
`perastage-cli` para automatización y flujos de trabajo avanzados. Ejecútela en
una terminal:

| Plataforma | Ejemplo |
| --- | --- |
| Windows (PowerShell, instalación predeterminada) | `& "C:\Program Files\Perastage\perastage-cli.exe" --help` |
| macOS (aplicación instalada en Applications) | `/Applications/Perastage.app/Contents/MacOS/perastage-cli --help` |
| Linux AppImage (sustituya el nombre por el archivo descargado) | `./Perastage-1.7.0-x86_64.AppImage --cli --help` |
| Arch Linux | `perastage-cli --help` |

Use `--version` para comprobar la versión instalada. Windows y macOS no añaden
la CLI a PATH. Los comandos y opciones siguen en inglés. `inspect` lee archivos
sin modificarlos; `scene` exige un archivo de salida separado y nunca sobrescribe
el origen; `live` solo se conecta a una instancia local de Perastage en ejecución
mediante loopback. Experimental describe la madurez del soporte y la estabilidad;
las reglas de validación y atomicidad existentes siguen vigentes. El adaptador
MCP se configura por separado y no está incluido en el instalador. Consulte la
gramática detallada en [Developer CLI](../../../developer/cli.md).

<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->

## Comandos habituales de Console

### Comandos de selección

| Comando | Descripción |
| --- | --- |
| `clear` | Vaciar las selecciones de luminarias, trusses y objetos |
| `f ...` | Seleccionar luminarias por ID |
| `t ...` | Seleccionar trusses por número de unidad (vacía antes la selección de trusses) |

Ejemplos de sintaxis de selección:

- ID único: `f 12`
- Intervalo: `f 1-5`, `f 1 thru 5`, `f 1 t 5`
- Añadir/quitar: `f + 10 - 3`
- Combinación: `f 1 3 5 7-9`

### Comandos de posición y rotación

| Comando | Descripción |
| --- | --- |
| `pos x <values>` | Establecer posiciones X |
| `pos y <values>` | Establecer posiciones Y |
| `pos z <values>` | Establecer posiciones Z |
| `pos <x>,<y>,<z>` | Establecer X/Y/Z conjuntamente |
| `x <values>` / `y <values>` / `z <values>` | Atajos por eje para `pos` |
| `rot x <values>` / `rot y <values>` / `rot z <values>` | Establecer alabeo/cabeceo/guiñada |
| `rot x\|y\|z <values> --group` o `--g` | Rotar toda la selección como un grupo |

Notas:

- Las posiciones y coordenadas del pivote de Console utilizan metros; las rotaciones utilizan grados.
- Un valor se aplica a todos los elementos seleccionados.
- Dos valores distribuyen linealmente la selección entre el inicio y el final; `t` y `thru` son separadores de intervalo opcionales, por lo que `pos x -7 t 7`, `pos x -7 thru 7` y `pos x -7 7` son equivalentes.
- Utiliza `++` / `--` para desplazamientos relativos. Se admiten valores con o sin espacio, por ejemplo `pos x ++1.5`, `pos x ++ 1.5`, `pos z --1` y `pos z -- 1`.
- El movimiento agrupado sigue las [preferencias de selección y movimiento](preferences.md#selección-y-movimiento).
- El pivote de rotación de grupo es, por defecto, el centro de la caja envolvente de la selección.
- Puedes cambiar el pivote con una terna final `x,y,z`, por ejemplo `rot y ++45 --g -2.5,0,0`.

### Espacios de transformación

Las transformaciones de la barra de comandos utilizan ejes globales por defecto. Los comandos de posición y rotación relativos pueden utilizar ejes locales con `--local` o `-l`:

```text
f 1
pos x ++ 1
pos x ++ 1 --local
rot y ++ 45
rot y ++ 45 -l
rot z ++ 30 --group --local
```

Los modificadores de espacio afectan a operaciones incrementales (`++` y `--`). Las posiciones absolutas siguen utilizando coordenadas globales y las rotaciones absolutas conservan el comportamiento de ángulos de Euler. El botón **Local Axes** conserva su estado y afecta solo a las transformaciones del visor; el historial y los scripts de la barra de comandos son deterministas y no heredan ese estado.

<!-- PERASTAGE_CONSOLE_HELP_END -->

## Foco y prioridad

- Los atajos se bloquean al escribir en campos o celdas editables.
- Los atajos de visor actúan en el visor que tiene el foco.
- Los atajos propios de un diálogo actúan solo en él.

## Atajos propios del diálogo Create from text

| Atajo | Acción |
| --- | --- |
| `↑` / `↓` | Recorrer sugerencias |
| `Enter` o `Tab` | Aceptar la sugerencia seleccionada |
| `Esc` | Cerrar sugerencias sin cerrar el diálogo |

Para editar páginas y utilizar el ratón en layouts, consulta [Layouts y PDF](layouts.md).
