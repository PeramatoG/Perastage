# Vistas

Utiliza 3D para comprobar relaciones espaciales y orientaciones, y 2D para revisar planos, posiciones y legibilidad. Muestra los visores ocultos desde **View**.

## Navegar y revisar

En 3D, arrastra para orbitar, usa Shift y arrastre o el botón central para desplazar, y la rueda para acercar o alejar. Haz clic derecho y elige **Render style** para cambiar la presentación, incluido el color por tipo, capa o universo.

En 2D, arrastra sobre un espacio vacío para desplazar y utiliza la rueda para acercar o alejar. Selecciona elementos para comprobar sus filas en Fixtures, Trusses, Hoists y Objects. Haz doble clic en una celda de una selección múltiple para editar esa columna en las filas seleccionadas.

## Herramienta de posición

Abre **View → Position Tool** en el espacio de trabajo 2D o 3D. Puedes acoplar, separar, redimensionar u ocultar el mismo panel; su posición y visibilidad se guardan con el espacio de trabajo. Sin selección, los campos están desactivados.

Los campos X, Y y Z comparten una sola fila de ancho flexible en un panel de altura compacta.

El panel aparece por defecto debajo de Summary, en la columna derecha de las vistas 2D y 3D. Las flechas de cada eje desplazan cada destino efectivo 1 mm por pulsación en métrico o 0,01 pies en imperial, conservando las diferencias y la precisión almacenada. Cada pulsación se puede deshacer.

X, Y y Z son coordenadas absolutas del punto de inserción en el mundo. Edita un campo y pulsa Intro o sal del campo para aplicarlo; los demás ejes, la rotación y la escala se conservan. Escape cancela la edición pendiente. Cada edición completada crea un paso de Deshacer. Las entradas vacías o no válidas no mueven nada.

En una selección múltiple se muestran las coordenadas comunes y se dejan vacías las que difieren. Introducir un valor asigna esa coordenada absoluta a cada destino efectivo por separado. Los hijos agrupados siguen las preferencias de **Selection & Movement**.

Pequeños cuadrados rojos indican los puntos de inserción efectivos en las vistas Superior, Inferior, Frontal y Lateral del visor 2D interactivo. Mantienen su tamaño en pantalla y no aparecen en capturas, planos impresos ni exportaciones. No hay indicadores en 3D.

Las unidades siguen **Preferences → Units**: metros con tres decimales o el formato imperial existente en pies. Se aceptan sufijos de unidad y entradas en pies/pulgadas. El redondeo visual no cambia las posiciones guardadas.

## Mover y seleccionar

- **Drag Move** permite arrastrar la selección con el botón izquierdo; está desactivado por defecto.
- **Axis Lock** restringe el movimiento. Al desactivarlo, permite movimiento libre en 2D o sobre un plano orientado a la cámara en 3D.
- **Local Axes** cambia los ejes de transformación del visor.
- **Magnet** ajusta a anclajes compatibles al mover o insertar elementos.
- **Cross-table Actions** permite que las herramientas del visor actúen sobre tipos de objeto fuera de la tabla activa; está desactivado por defecto.

El movimiento agrupado sigue las [preferencias de selección y movimiento](preferences.md#selección-y-movimiento). Revisa la posición final en ambas vistas y utiliza Undo si es necesario.

Consulta [Atajos y barra de comandos](shortcuts-and-command-bar.md) para teclado, ratón y Console. Para páginas imprimibles, utiliza [Layouts y PDF](layouts.md).

## Alinear objetos de escena

Use la barra **Alineación** del espacio 2D o 3D. Seleccione al menos dos destinos efectivos en los visores o las tablas. Elija **Extremos**, **Primero seleccionado** o **Último seleccionado** y pulse **Alinear arriba**, **Alinear abajo**, **Alinear a la izquierda**, **Alinear a la derecha**, **Centrar horizontalmente** o **Centrar verticalmente**. La referencia primero/último sigue el orden de selección entre tipos y permanece fija. Quitar y volver a seleccionar un objeto lo convierte en el más reciente. Las selecciones masivas nuevas siguen el orden estable de UUID; ordenar las tablas conserva el orden anterior.

La referencia y el plano son grupos de iconos exclusivos: siempre hay una opción activa en cada grupo. Pase el ratón sobre un icono para ver su ayuda. En 2D, el plano sigue automáticamente la vista: **XY** para Superior, **XZ** para Frontal y **YZ** para Lateral. En 3D, los botones **XY/XZ/YZ** comparten el plano entre alineación y arrastre de selecciones. Las vistas estándar actualizan el plano; orbitar en perspectiva lo conserva. El plano inicial es XY. La alineación conserva la tercera coordenada, la rotación y la escala; Lateral usa Y reflejado. Bloquear ejes elige una dirección dentro del plano; Ejes locales proyecta las direcciones locales sobre él. El arrastre libre usa el plano que pasa por la selección. Si se ve de canto, cambie la vista u orbite. Magnet es una excepción: puede salir del plano para ajustarse a un elemento cercano. La inserción continua y la distribución conservan su comportamiento. Los grupos siguen las preferencias de movimiento y nunca se desplazan dos veces sus descendientes seleccionados.

Cada alineación con cambios crea un paso de Deshacer; repetirla sin cambios no crea historial. En el modo de límites geométricos, si falta geometría física, se cancela toda la operación: el diagnóstico identifica el UUID/recurso. Los motores ficticios sin geometría física no admiten alineación por extremos. La alineación de Layout no está incluida.

Elige **Alinear usando límites geométricos** (predeterminado) para alinear bordes y centros físicos, o **Alinear usando puntos de inserción** para alinear los orígenes mundiales. Estos dos iconos son exclusivos y funcionan con todas las referencias y planos. Con referencia Primero/Último y puntos de inserción, Izquierda/Derecha/Centrar horizontalmente alinean la misma coordenada horizontal; Superior/Inferior/Centrar verticalmente, la vertical. Extensiones usa el mínimo/máximo de los puntos o su punto medio para centrar. Los grupos usan su propio origen mundial y desplazan juntos a sus descendientes. Este modo no necesita geometría y se elige explícitamente; nunca se activa automáticamente si faltan límites físicos.
