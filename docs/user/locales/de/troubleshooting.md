# Fehlerbehebung

## Probleme beim Öffnen und Importieren von MVR

Prüfen Sie die Endung `.mvr`, testen Sie eine andere Datei zur Eingrenzung und lesen Sie Importmeldungen in der Console. Der [Inspektor](inspector.md) prüft externe Pakete ohne Import. Bei Zusammenführungskonflikten siehe [MVR-Dateien öffnen](opening-mvr-files.md).

## Fehlende oder falsche GDTF-Scheinwerfer

Prüfen Sie Definition und ausgewählten DMX-Modus. Nutzen Sie **Tools → Download GDTF** für Profile und **Tools → Edit dictionaries** für lokale Zuordnungen. Öffnen und prüfen Sie die Szene nach Profilkorrekturen erneut. Prüfen Sie das ausgewählte Model im Fixture Editor; Projektsymbole liegen unabhängig in PSTG.

## Benutzerbibliothek

Nutzen Sie **Tools → Open user library folder**; unter Windows ist die Vorgabe `%APPDATA%\Perastage\library\`. Prüfen Sie die Schreibbarkeit Ihres Benutzerprofils und sichern Sie eigene Inhalte vor Dateiersatz. Bearbeiten Sie Benutzerdateien statt installierter Standards.

## Layout- und PDF-Probleme

Wählen Sie ein nicht leeres Layout, prüfen Sie Rahmen, Ausrichtung sowie Szenen-/Tabelleninhalt vor **File → Print Layout...**. Wählen Sie ein beschreibbares Ziel und einen nicht leeren Dateinamen. Prüfen Sie das PDF; den Ablauf beschreibt [Layouts und PDF](layouts.md).

## Projekt- und Laufzeitprobleme

Speichern Sie möglichst eine separate Projektkopie, starten Sie Perastage neu und öffnen Sie sie erneut. Prüfen Sie kritische Daten in 2D, 3D und Tabellen. Bei langsamer 3D-Navigation reduzieren Sie [Modelldetails](preferences.md#3d-ansicht) oder aktivieren Sie das Navigationsersatzmodell. Vergleichen Sie mit einem kleinen Projekt zur Eingrenzung szenenspezifischer Probleme.

## GDTF-Share-Zugangsdaten

Online-Abrufe und Downloads benötigen Konto und Netzwerkzugang; [Cache-Suche](gdtf-download.md#zwischengespeicherte-und-offline-suche) bleibt offline verfügbar.

Warnt Perastage vor einem nicht verfügbaren sicheren Zugangsdaten-Speicher, kann das Passwort für den aktuellen Vorgang verwendet werden, bleibt aber nicht gespeichert. Prüfen Sie den Betriebssystemspeicher; Linux benötigt einen aktiven Secret-Service-Anbieter wie GNOME Keyring oder KWallet. Bleibt die Warnung in einem Veröffentlichungspaket bestehen, nehmen Sie sie in den Bericht auf. Geben Sie keine Passwörter weiter.

## Protokolle und Diagnoseberichte

Mit **Help → Open Logs Folder** finden Sie `perastage.log`, das Protokoll des vorherigen Starts `perastage.previous.log` und den Unterordner `crash_reports`.

Standardpfade:

- Windows: `%LOCALAPPDATA%\Perastage\logs\`
- macOS: `~/Library/Logs/Perastage/`
- Linux: `${XDG_STATE_HOME}/perastage/logs/`, oder `~/.local/state/perastage/logs/`, falls nicht gesetzt.

**Help → Export Diagnostic Report...** erstellt einen Textbericht mit Build-/Plattformdaten, verfügbaren OpenGL-Informationen und aktuellen Protokollzeilen. Berichte bleiben lokal und werden nicht automatisch hochgeladen. Prüfen Sie den Bericht vor Weitergabe und ergänzen Sie Reproduktionsschritte sowie möglichst eine kleine Beispieldatei.

Fehler bei Quellcode-Builds, Abhängigkeiten und Werkzeugketten behandelt die [Entwickler-Build-Anleitung](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md).
