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
