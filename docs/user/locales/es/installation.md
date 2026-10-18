# Instalación

Descarga desde la [web oficial](https://perastage.luismaperamato.com/#download), con descargas por plataforma y notas de versión. Si los metadatos no están disponibles, utiliza la [última versión de GitHub](https://github.com/PeramatoG/Perastage/releases/latest). Los paquetes disponibles pueden variar.

## Windows

1. Descarga el instalador oficial x64 y ejecuta el archivo de instalación.
2. SmartScreen puede advertir que la aplicación no se reconoce. Verifica que procede de la versión oficial. Si confías en ella, selecciona **More info → Run anyway**, cuando esté disponible.
3. Completa la instalación y abre Perastage desde el acceso directo.

No desactives Windows Defender ni SmartScreen.

## macOS

1. Elige **macOS 15 — Apple Silicon (Compatibility Build)** para macOS 15 o **macOS 26 — Apple Silicon** para macOS 26. Ambos paquetes son ARM64; no admiten Mac Intel.
2. Abre el DMG y arrastra Perastage a Applications.
3. Intenta abrir Perastage.
4. Las compilaciones actuales no tienen firma Developer ID ni notarización de Apple. Si Gatekeeper bloquea la aplicación porque no puede verificar al desarrollador y confías en la descarga oficial, utiliza **System Settings → Privacy & Security → Open Anyway**, cuando esté disponible.

Consulta las [instrucciones de Apple](https://support.apple.com/en-us/102445). Mantén Gatekeeper activado.

### Solución avanzada de problemas de macOS

Algunas versiones pueden indicar que la aplicación está dañada. Primero vuelve a descargarla desde la versión oficial. Solo tras verificar su procedencia oficial y confiar en el archivo, los usuarios avanzados pueden ejecutar:

```bash
xattr -dr com.apple.quarantine /Applications/Perastage.app
```

Esto elimina recursivamente la cuarentena de esta aplicación y omite su comprobación de Gatekeeper basada en cuarentena. No verifica la aplicación ni repara archivos dañados. No lo uses con descargas no fiables ni elimines la cuarentena globalmente.

## Linux AppImage

1. Descarga la AppImage x86_64.
2. En la carpeta de descarga, dale permiso de ejecución:

```bash
chmod +x Perastage-*.AppImage
```

3. Abre la AppImage para iniciar Perastage. Normalmente no necesita instalación.

## Arch Linux (experimental)

El paquete nativo x86_64 es experimental. En la carpeta del paquete descargado, instálalo con:

```bash
sudo pacman -U ./Perastage-*-arch-x86_64.pkg.tar.zst
```

Después abre Perastage desde el menú de aplicaciones.

## Tu biblioteca editable

Utiliza **Tools → Open user library folder** para localizar los diccionarios, perfiles de luminarias y demás contenido que puedes editar. En Windows, la ruta predeterminada es `%APPDATA%\Perastage\library\`.

Haz copias de seguridad de tu contenido personalizado. Los valores incluidos con la aplicación completan los archivos que falten sin sobrescribir los existentes; edita la copia de usuario, no la carpeta de instalación.

Si hay problemas al iniciar, importar o localizar contenido, consulta [Resolución de problemas](troubleshooting.md). Para compilar desde el código fuente, consulta la [guía de compilación](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md).
