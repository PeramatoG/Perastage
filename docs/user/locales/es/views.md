# Vistas

Utiliza 3D para comprobar relaciones espaciales y orientaciones, y 2D para revisar planos, posiciones y legibilidad. Muestra los visores ocultos desde **View**.

## Navegar y revisar

En 3D, arrastra para orbitar, usa Shift y arrastre o el botón central para desplazar, y la rueda para acercar o alejar. Haz clic derecho y elige **Render style** para cambiar la presentación, incluido el color por tipo, capa o universo.

En 2D, arrastra sobre un espacio vacío para desplazar y utiliza la rueda para acercar o alejar. Selecciona elementos para comprobar sus filas en Fixtures, Trusses, Hoists y Objects. Haz doble clic en una celda de una selección múltiple para editar esa columna en las filas seleccionadas.

## Herramienta de posición

Abre **View → Position Tool** en el espacio de trabajo 2D o 3D. Puedes acoplar, separar, redimensionar u ocultar el mismo panel; su posición y visibilidad se guardan con el espacio de trabajo. Sin selección, los campos están desactivados.

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
