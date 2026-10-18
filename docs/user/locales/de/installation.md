# Installation

Laden Sie Perastage von der [offiziellen Website](https://perastage.luismaperamato.com/#download) mit Plattformdownloads und Versionshinweisen herunter. Falls die Metadaten fehlen, verwenden Sie die [neueste GitHub-Version](https://github.com/PeramatoG/Perastage/releases/latest). Verfügbare Pakete können variieren.

## Windows

1. Laden Sie das offizielle x64-Installationsprogramm herunter und führen Sie es aus.
2. SmartScreen kann vor einer unbekannten App warnen. Prüfen Sie die offizielle Herkunft. Wenn Sie der Datei vertrauen, wählen Sie **More info → Run anyway**, sofern verfügbar.
3. Schließen Sie die Installation ab und starten Sie Perastage über die installierte Verknüpfung.

Deaktivieren Sie weder Windows Defender noch SmartScreen.

## macOS

1. Wählen Sie **macOS 15 — Apple Silicon (Compatibility Build)** für macOS 15 oder **macOS 26 — Apple Silicon** für macOS 26. Beide Pakete sind für ARM64; Intel-Macs werden nicht unterstützt.
2. Öffnen Sie das DMG und ziehen Sie Perastage nach Applications.
3. Versuchen Sie, Perastage zu starten.
4. Aktuelle Builds sind weder mit Developer ID signiert noch von Apple notarisiert. Wenn Gatekeeper die App wegen eines nicht überprüfbaren Entwicklers blockiert und Sie dem offiziellen Download vertrauen, verwenden Sie **System Settings → Privacy & Security → Open Anyway**, sofern verfügbar.

Siehe [Apples Anleitung](https://support.apple.com/en-us/102445). Lassen Sie Gatekeeper aktiviert.

### Erweiterte macOS-Fehlerbehebung

Einige Versionen melden möglicherweise eine beschädigte App. Laden Sie sie zunächst erneut aus der offiziellen Veröffentlichung herunter. Nur nachdem die offizielle Herkunft geprüft wurde und Sie der Datei vertrauen, können erfahrene Benutzer Folgendes ausführen:

```bash
xattr -dr com.apple.quarantine /Applications/Perastage.app
```

Dies entfernt rekursiv die Quarantäne dieser App und umgeht deren quarantänebasierte Gatekeeper-Prüfung. Es überprüft die App nicht und repariert keine beschädigten Dateien. Verwenden Sie es niemals für nicht vertrauenswürdige Downloads und entfernen Sie Quarantäne nicht global.

## Linux AppImage

1. Laden Sie das x86_64-AppImage herunter.
2. Machen Sie es im Downloadordner ausführbar:

```bash
chmod +x Perastage-*.AppImage
```

3. Öffnen Sie das AppImage zum Starten. Eine Installation ist normalerweise nicht erforderlich.

## Arch Linux (experimentell)

Das native x86_64-Paket ist experimentell. Installieren Sie es im Ordner des heruntergeladenen Pakets mit:

```bash
sudo pacman -U ./Perastage-*-arch-x86_64.pkg.tar.zst
```

Starten Sie Perastage anschließend über das Anwendungsmenü.

## Ihre bearbeitbare Bibliothek

Mit **Tools → Open user library folder** finden Sie beschreibbare Verzeichnisse, Scheinwerferprofile und weitere Bibliotheksinhalte. Unter Windows ist der Standardpfad `%APPDATA%\Perastage\library\`.

Sichern Sie eigene Bibliotheksinhalte. Mitgelieferte Standarddateien ergänzen fehlende Dateien, ohne vorhandene Benutzerdateien zu überschreiben. Bearbeiten Sie die Benutzerkopie anstelle des Installationsverzeichnisses.

Bei Start-, Import- oder Ressourcenproblemen siehe [Fehlerbehebung](troubleshooting.md). Quellcode-Builds beschreibt die [Entwickler-Build-Anleitung](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md).
