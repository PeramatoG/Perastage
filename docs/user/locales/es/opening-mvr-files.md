# Abrir archivos MVR

## Abrir o importar

Utiliza **File → Import MVR...** y elige:

- **Open as new project** para sustituir la escena tras las comprobaciones habituales de cambios sin guardar.
- **Merge into current project** para añadir contenido conservando tu trabajo.

Abrir un `.mvr` directamente al iniciar o mediante una asociación de archivos compatible del sistema operativo lo abre como proyecto nuevo. Cancelar la elección de importación conserva el proyecto actual.

Revisa la escena importada en [2D/3D](views.md) y en las tablas Fixtures, Trusses, Hoists y Objects. Utiliza [Inspector](inspector.md) para examinar un paquete sin importarlo.

## Conflictos al fusionar

Si coinciden UUIDs, puedes generar UUIDs nuevos para los objetos entrantes (opción predeterminada, que conserva ambas escenas), sustituir los objetos coincidentes del proyecto u omitir los objetos entrantes en conflicto.

Si un nombre de tipo de luminaria corresponde a otra definición GDTF o modo, elige la definición del proyecto, conserva la importada con otro nombre de tipo o cancela la fusión. Una fusión cancelada o fallida restaura el proyecto actual.

Las direcciones DMX duplicadas generan avisos en Console que no bloquean la operación. Revisa y corrige el patch después de fusionar.

## Recursos y almacenamiento del proyecto

Los GDTF, símbolos y modelos importados se conservan entre los recursos del proyecto para poder localizarlos al guardar y volver a abrir. Comprueba los perfiles y modos que falten antes de exportar; consulta [Descargar GDTF](gdtf-download.md) y [Resolución de problemas](troubleshooting.md).

Guarda el trabajo en `.pstg`, que conserva el contexto de Perastage, los layouts y los símbolos del proyecto. MVR es el formato de intercambio. Perastage guarda sus metadatos de intercambio en UserData propio compatible con el estándar; otras aplicaciones pueden no conservarlos.

## Exportar MVR

1. Elige **File → Export MVR...**.
2. Elige el destino; si existe un nombre de proyecto, se utiliza para sugerir el nombre de archivo.
3. Exporta y comprueba el resultado en la aplicación de destino.

[Preferencias](preferences.md#importación--exportación-mvr) ofrece una opción de compatibilidad para la geometría de trusses. Los símbolos del proyecto guardados en PSTG no se exportan a MVR ni a los GDTF incrustados.
