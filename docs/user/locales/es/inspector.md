# MVR / GDTF Inspector

Abre **View → Layout Views → MVR / GDTF Inspector** para examinar una instantánea del proyecto actual. La inspección es de solo lectura: no modifica el proyecto ni el archivo inspeccionado, no cambia la selección y no marca el proyecto como editado.

## Elegir qué inspeccionar

- La vista del proyecto actual es una instantánea, no una escena que se actualice continuamente. Utiliza **Refresh** después de editar para capturar el estado más reciente.
- Utiliza **Open external...** para inspeccionar un `.mvr` o `.gdtf` externo sin importarlo.

## Explorar contenido MVR y GDTF

En MVR, recorre el árbol del paquete para inspeccionar sus archivos, o el árbol de escena para examinar los objetos y sus nombres, tipos y UUIDs. Activa un GDTF incrustado para abrir su definición de luminaria y vuelve después al MVR padre.

En GDTF, revisa la identidad de FixtureType, la información física y las revisiones. Selecciona un modo DMX para inspeccionar sus canales, funciones y conjuntos. Revisa ruedas, ranuras, filtros y referencias a recursos en las páginas de detalle.

## Código fuente y vista previa

Selecciona un recurso del paquete para examinar su contenido o vista previa. XML y texto sin formato aparecen en **Source**; SVG ofrece XML y una vista renderizada. Las imágenes compatibles y los modelos GLB/3DS aparecen en **Preview**. Los recursos no compatibles o demasiado grandes siguen en la lista con su estado de vista previa.

Utiliza los controles de búsqueda, plegado y copia para examinar XML. Los documentos grandes pueden abrirse con una vista limitada y ofrecer una acción explícita para cargar todo el texto. Arrastra los separadores para dar más espacio a la navegación, el código fuente, los detalles o la vista previa.

## Incidencias y diagnóstico

**Issues** agrupa los problemas detectados; **Diagnostics** aporta más detalle para investigarlos. Una limitación de la vista previa no implica que el archivo sea inválido. Inspector no repara paquetes ni descarga recursos que falten. Utiliza los flujos de edición habituales para corregirlos, o [exporta un informe de diagnóstico](troubleshooting.md#registros-e-informes-de-diagnóstico) al comunicar una incidencia.
