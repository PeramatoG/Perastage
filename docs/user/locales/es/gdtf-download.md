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
