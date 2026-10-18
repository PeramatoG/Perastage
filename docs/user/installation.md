# Installation

Download from the [official website](https://perastage.luismaperamato.com/#download). It offers direct platform downloads and release notes; if metadata is unavailable, use the [latest GitHub release](https://github.com/PeramatoG/Perastage/releases/latest). Package availability can vary by release.

## Windows

1. Download the official x64 installer and run the setup file.
2. Windows SmartScreen may warn about an unrecognized app. Verify that the file came from the official release. If you trust it, choose **More info → Run anyway**, when available.
3. Complete installation and launch Perastage from the installed shortcut.

Do not disable Windows Defender or SmartScreen.

## macOS

1. Choose **macOS 15 — Apple Silicon (Compatibility Build)** for macOS 15 or **macOS 26 — Apple Silicon** for macOS 26. Both builds target ARM64; Intel Macs are not supported by these packages.
2. Open the DMG and drag Perastage into Applications.
3. Try to launch Perastage.
4. Current builds are not Developer ID signed or notarized by Apple. If Gatekeeper blocks the app because the developer cannot be verified, and you trust the official download, use **System Settings → Privacy & Security → Open Anyway**, when available.

See [Apple’s guidance on opening apps safely](https://support.apple.com/en-us/102445). Keep Gatekeeper enabled.

### Advanced macOS troubleshooting

Some macOS versions may say the app is damaged. Download it again from the official release first. Only after verifying its official source and trusting the file, advanced users can run:

```bash
xattr -dr com.apple.quarantine /Applications/Perastage.app
```

This recursively removes quarantine from this app and bypasses the quarantine-based Gatekeeper check for it. It does not verify the app or repair corruption. Never use it on an untrusted download or remove quarantine globally.

## Linux AppImage

1. Download the x86_64 AppImage.
2. In the download folder, make it executable:

```bash
chmod +x Perastage-*.AppImage
```

3. Open the AppImage to launch Perastage. AppImage normally requires no installation.

## Arch Linux (experimental)

The native x86_64 package is experimental. In the folder containing the downloaded package, install it with:

```bash
sudo pacman -U ./Perastage-*-arch-x86_64.pkg.tar.zst
```

Then launch Perastage from the application menu.

## Your editable library

Use **Tools → Open user library folder** to find writable dictionaries, fixture profiles and other library content. On Windows the default is `%APPDATA%\Perastage\library\`.

Keep custom library content backed up. Bundled defaults fill missing user-library files without overwriting your existing files; edit the user copy rather than the installation directory.

For launch, import or missing-content problems, see [Troubleshooting](troubleshooting.md). Source-build instructions belong in the [developer build guide](../developer/build.md).
