# Packaging and Platform Integration

This document describes the supported distribution and file-association behavior for Perastage.

## Windows Packaging

The official Windows installer workflow uses Inno Setup:

- Script: `packaging/windows/Perastage.iss`
- Build Release and run `perastage_stage` first.
- Compile the installer script in Inno Setup.
- Optional associations can include `.pstg` and `.mvr`.

### Association Notes

- Installer writes association entries under `Software\Classes`.
- Uninstall behavior is conservative and avoids aggressive global extension ownership cleanup.
- Legacy CPack/NSIS wiring exists for compatibility but is not the primary path.

## Linux Desktop Integration

Install layouts include desktop and MIME metadata:

- `share/applications/perastage.desktop`
- `share/mime/packages/perastage-mime.xml`
- `share/icons/hicolor/1024x1024/apps/perastage.png`

During `cmake --install`, cache refresh commands may run to expose new associations.

## macOS Document Association

Perastage app bundles declare `.mvr` document type metadata through bundle settings so Finder can route files to the application.

## Related Documents

- [Build and dependency guide](build.md)
- [Troubleshooting](troubleshooting.md)
