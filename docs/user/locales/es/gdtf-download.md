# Descargar GDTF

Utiliza **Tools → Download GDTF** para buscar y descargar perfiles de luminarias de GDTF Share. Cargar el catálogo en línea y descargar perfiles requiere una cuenta de GDTF Share y conexión a Internet.

## Buscar un perfil

1. Abre **Tools → Download GDTF**. Si hay un catálogo en caché válido, se abre inmediatamente; en caso contrario, inicia sesión para cargarlo. Cancelar este inicio de sesión cancela la operación.
2. Busca por fabricante, luminaria, UUID, revisión o nombre de modo DMX. Todas las palabras deben coincidir, en cualquier orden; los filtros Manufacturer y Fixture acotan los resultados.
3. Selecciona una revisión y revisa su autor, fechas, tamaño, identificadores y modos DMX con el número de canales que ocupan antes de descargar.
4. Descarga el perfil y comprueba la definición y el modo de la luminaria en el proyecto o diccionario.

## Consulta en caché y sin conexión

Un catálogo reciente se reutiliza durante una hora. Los datos antiguos se actualizan en segundo plano mientras puedes seguir consultándolos; si falla la actualización, los resultados en caché siguen disponibles. La consulta en caché funciona sin conexión y sin iniciar sesión. Las descargas requieren autenticación en línea y esperan a que termine una actualización en curso.

Una descarga fallida o cancelada conserva el perfil que ya exista en el destino.

## Credenciales y perfiles locales

Las contraseñas se guardan en el almacén seguro de credenciales del sistema operativo cuando está disponible. Si no lo está, Perastage avisa y permite utilizar las credenciales para la operación actual sin guardar la contraseña. Puede ser necesario introducirla de nuevo en otra sesión. El nombre de usuario puede recordarse como ayuda para iniciar sesión.

Utiliza **Tools → Edit dictionaries** para gestionar las asignaciones de perfiles y **Tools → Open user library folder** para localizar contenido local. Consulta los [problemas con credenciales](troubleshooting.md#credenciales-de-gdtf-share) si no se conservan las contraseñas.

## Resolver perfiles al importar un MVR

El diálogo MVR/App/Download se mantiene. Download abre la cola con sugerencias
seguras y los perfiles MVR o de la aplicación disponibles como alternativa.
Seleccione una fila para revisar revisiones y advertencias, incluso después de
una descarga correcta. **Aplicar selecciones** procesa los cambios antes de que
**Aceptar** aplique el resultado a todas las instancias de ese tipo.

Elegir **Download** solicita el archivo del repositorio cuando el modelo coincide
sin ambigüedad, aunque el nombre del modo sea distinto. Por ejemplo, `DMX Mode`
no impide descargar ELP PAR con los modos `Basic` y `Compact`. Confirme el modo
real después de descargar cuando deba revisarse la compatibilidad. Si cancela esa
elección, **Descargado; seleccione el modo DMX** conserva el paquete. Utilice
**Elegir modo DMX...** más tarde o seleccione otro origen. Aceptar permanece
deshabilitado mientras haya modos pendientes.

Cada fila tiene casillas exclusivas **MVR**, **Perastage** y **Archivo seleccionado**.
Los orígenes no disponibles están deshabilitados. MVR conserva los modos importados;
Perastage utiliza un perfil disponible de la biblioteca; Archivo seleccionado
recupera la última elección descargada o local. Volver al descargado reutiliza su
paquete validado. **Descargado; idéntico al MVR original** indica que se han comparado
los bytes del archivo descargado y del perfil original y coinciden exactamente;
el nombre o FixtureTypeID compartidos no demuestran que los archivos sean iguales.

**Buscar en GDTF Share...** reutiliza el navegador completo. **Cargar GDTF local...**
valida el archivo y copia el perfil a los recursos de la importación sin modificar
el original. Elija el modo DMX y revise las diferencias de modelo, modo y huella.
También puede seleccionar explícitamente un perfil alternativo disponible.

El cierre automático solo se configura en **Preferencias → MVR Import/Export**
y sigue desactivado por defecto. Al activarlo, las selecciones seguras completadas
continúan la importación; las ambigüedades y los errores permanecen visibles.

La cola muestra actividad del catálogo y del acceso, tiempo transcurrido, progreso
por tipo de luminaria y bytes descargados. Verde indica descarga, reutilización o
selección local correcta; ámbar indica perfil original del MVR o alternativa de la
aplicación; rojo indica error o perfil sin resolver.

Los candidatos muestran todos sus modos y canales disponibles. Los datos ausentes
se indican como no disponibles, en lugar de cero canales, y se actualizan al leer
el paquete descargado. Los detalles muestran el modo importado y su consumo conocido.
La separación repetida del patch puede sugerir un modo en el selector, pero exige
confirmación: puede incluir canales libres y una huella igual no garantiza las
mismas funciones DMX.

**Cancelar**, **Escape** y cerrar la ventana detienen la cola y continúan la
importación con los perfiles y modos originales del MVR para todas sus filas.
Se descartan todas las sustituciones de la cola, incluidas las descargas terminadas
y las selecciones locales. **Aceptar** confirma los resultados.

**Aplicar selecciones** también reintenta las descargas fallidas. Cancelar la autenticación detiene los intentos pendientes sin repetir el diálogo de acceso por cada fila.
