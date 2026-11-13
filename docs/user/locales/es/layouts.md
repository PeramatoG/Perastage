# Layouts y PDF

Los layouts organizan las vistas de escena y la información complementaria en páginas imprimibles.

## Preparar una página

1. Abre **View → Layout Views → Layout Mode View**. Si es necesario, muestra **Layouts** desde **View**.
2. Selecciona un layout o utiliza **Add** para crear uno. **Rename** y **Delete** gestionan las páginas; no se puede eliminar el último layout.
3. Utiliza el menú contextual del layout para elegir la orientación.
4. Añade elementos desde la barra de Layout:

| Acción | Contenido |
| --- | --- |
| **Add 2D View** | Vista 2D de la escena |
| **Add Legend** | Leyenda de luminarias |
| **Add Event Table** | Información del evento |
| **Add Text** | Notas y títulos |
| **Add Image** | Imágenes, como logotipos |

Edita los ajustes disponibles de cada elemento para elegir contenido y presentación. Comprueba los datos en las [vistas y tablas](views.md) antes de generar documentación.

## Organizar y editar

Arrastra un marco para moverlo y uno de sus tiradores para cambiar su tamaño. Haz doble clic para editar el elemento. Haz clic derecho para editar, eliminar y cambiar el orden de superposición. Arrastra sobre un espacio vacío para desplazar y usa la rueda para acercar o alejar. **Z** ajusta el layout a la vista; **Delete** elimina el elemento seleccionado.

Guarda el proyecto `.pstg` para conservar los layouts. **Export template** crea un paquete portátil `.pslayout` con las imágenes referenciadas. **Import template** acepta `.pslayout` y plantillas antiguas `.json`.

## Exportar PDF

Selecciona un layout con contenido y elige **File → Print Layout...**. Elige un destino `.pdf` con permisos de escritura. El PDF utiliza el tamaño y la orientación del layout seleccionado e incluye sus elementos compatibles. Revisa el archivo antes de imprimir o compartir.

Si faltan vistas, el resultado es incorrecto o falla la exportación, consulta [Resolución de problemas](troubleshooting.md#problemas-con-layouts-y-pdf).

## Escala física y propiedades de la vista

Haz doble clic en una vista 2D y abre **Propiedades de la vista**. **Libre / Variable** sigue siendo el modo predeterminado para vistas nuevas y archivos antiguos: permite encuadrar con zoom, desplazamiento y ajuste visual. La escala efectiva aproximada es informativa. El control **Encuadre visual** modifica el encuadre visual, no una escala física.

Activa **Fija** para elegir 1:25, 1:50, 1:100, 1:200 o 1:500, o un denominador entre 0,01 y 10000000. Un metro ocupa 20 mm a 1:50 y 10 mm a 1:100. El centro usa metros y los ejes superior X/Y, frontal X/Z o lateral Y/Z; en lateral, la dirección horizontal de pantalla es −Y. Introduce el centro o desplaza directamente la vista en el editor. En modo fijo, el marco rojo muestra el recorte real a la escala elegida. El zoom, el ajuste automático y el control de encuadre quedan bloqueados; arrastrar o usar las flechas cambia el centro sin alterar la escala. Al volver a libre se recupera el encuadre anterior.

Los milímetros se muestran sin decimales. Se pueden introducir fracciones y los campos sin modificar conservan toda su precisión interna.

Las flechas ajustan el tamaño del marco en pasos de 1 mm, el centro en pasos de 0,01 m y el denominador de escala de uno en uno, conservando las fracciones internas. También puedes escribir valores personalizados o elegir una escala común. En modo fijo, un aviso visible muestra la escala y explica que el zoom está bloqueado.

Los cambios válidos en **Propiedades de la vista** se previsualizan inmediatamente en el editor, incluidos el modo, la escala, el centro y el tamaño del marco. Mientras se escribe un número incompleto o no válido, se mantiene la última previsualización válida. **Cancel** o cerrar el diálogo restaura las propiedades y el encuadre anteriores; **OK** conserva los cambios en el borrador del editor.

El ancho y alto del marco usan milímetros y conservan la precisión fraccionaria. Para cambiar la posición, mueve el elemento en la página del layout; X/Y no aparecen en este diálogo. Se admiten posiciones parcialmente fuera de página. El tamaño mínimo es 1 punto (0,353 mm) y el límite es 200000 puntos. Mover o redimensionar una vista fija conserva centro y escala, cambiando el área visible. El zoom del layout y los DPI solo afectan a la visualización. Volver a libre conserva la cámara anterior, filtros y presentación. **Cancel** descarta el borrador; **OK** confirma una operación con Deshacer/Rehacer y persistencia PSTG/PSLAYOUT.

El menú contextual de la página permite A3/A4 y orientación sin escalar elementos. Las páginas con vistas fijas requieren PDF al 100 % y el mismo papel del layout; otro tamaño se rechaza con una explicación. Para usar otro papel, cambia primero el papel del layout. Las páginas libres conservan el ajuste uniforme de salida. Imprime el PDF a tamaño real/100 %: Perastage no controla el escalado del controlador. La escala física garantiza geometría, sin ampliar las garantías de tipografía o estilos existentes.
