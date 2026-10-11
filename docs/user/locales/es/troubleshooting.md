# Resolución de problemas

## Problemas al abrir e importar MVR

Comprueba la extensión `.mvr`, prueba otro archivo para aislar el problema y revisa los mensajes de importación en Console. Utiliza [Inspector](inspector.md) para examinar un paquete externo sin importarlo. Para conflictos de fusión, consulta [Abrir archivos MVR](opening-mvr-files.md).

## Luminarias GDTF ausentes o incorrectas

Comprueba la definición de la luminaria y el modo DMX seleccionado. Utiliza **Tools → Download GDTF** para perfiles y **Tools → Edit dictionaries** para asignaciones locales. Vuelve a abrir y revisar la escena tras corregir los perfiles que falten. Comprueba la [política de completado](preferences.md#completar-definiciones-gdtf) si las vistas generadas no son las esperadas.

## Biblioteca de usuario

Utiliza **Tools → Open user library folder**; en Windows la ruta predeterminada es `%APPDATA%\Perastage\library\`. Comprueba los permisos de escritura de tu perfil y haz una copia del contenido personalizado antes de sustituir archivos. Edita los archivos de usuario, no los valores instalados.

## Problemas con layouts y PDF

Selecciona un layout con contenido, comprueba los marcos y la orientación y verifica los datos de escena y tablas antes de **File → Print Layout...**. Elige un destino con permisos de escritura y un nombre de archivo no vacío. Revisa el PDF exportado; consulta [Layouts y PDF](layouts.md) para el flujo de trabajo.

## Problemas del proyecto y de ejecución

Si puedes, guarda una copia independiente del proyecto, reinicia Perastage y vuelve a abrirla. Revisa los datos críticos en 2D, 3D y tablas. Si la navegación 3D es lenta, reduce [Model detail](preferences.md#3d-viewer) o activa la representación simplificada durante la navegación. Compara con un proyecto pequeño para aislar problemas propios de la escena.

## Credenciales de GDTF Share

Las cargas en línea y las descargas requieren cuenta y acceso a la red; la [consulta del catálogo en caché](gdtf-download.md#consulta-en-caché-y-sin-conexión) sigue disponible sin conexión.

Si Perastage avisa de que el almacén seguro de credenciales no está disponible, puedes utilizar la contraseña en la operación actual, pero no se conserva. Comprueba el almacén del sistema operativo; Linux necesita un proveedor activo de Secret Service, como GNOME Keyring o KWallet. Si el aviso persiste en un paquete de distribución, inclúyelo en el informe. No incluyas contraseñas.

## Registros e informes de diagnóstico

Utiliza **Help → Open Logs Folder** para localizar `perastage.log`, `perastage.previous.log` del inicio anterior y la subcarpeta `crash_reports`.

Rutas predeterminadas:

- Windows: `%LOCALAPPDATA%\Perastage\logs\`
- macOS: `~/Library/Logs/Perastage/`
- Linux: `${XDG_STATE_HOME}/perastage/logs/`, o `~/.local/state/perastage/logs/` si la variable no está definida.

Utiliza **Help → Export Diagnostic Report...** para generar un informe de texto con datos de compilación y plataforma, información OpenGL disponible y líneas recientes del registro. El informe permanece local y no se sube automáticamente. Revísalo antes de compartirlo, junto con los pasos para reproducir el problema y un archivo pequeño de ejemplo si es posible.

Los problemas de compilación, dependencias y herramientas se tratan en la [guía para desarrolladores](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md).
