# Funktionsübersicht

Perastage ist eine Open-Source-Desktopanwendung für Licht- und Rigging-Szenen sowie technische Dokumentation.

## MVR-Projektabläufe

[MVR öffnen, zusammenführen und exportieren](opening-mvr-files.md), Konflikte prüfen und laufende Arbeiten als PSTG-Projekte speichern.

## GDTF und GDTF Share

Nutzen Sie Scheinwerferdefinitionen und lokale Verzeichnisse, [suchen und laden Sie GDTFs](gdtf-download.md), durchsuchen Sie den Cache offline und prüfen Sie Revisionen und DMX-Modi. Der Fixture Editor prüft Models und erhält ursprüngliche GDTF-Ressourcen.

## MVR / GDTF-Inspektor

[Prüfen Sie](inspector.md) Projektstände und externe Pakete ohne Änderungen. Durchsuchen Sie Szenen-/Paketbäume, Definitionen, XML, Vorschauen und gemeldete Probleme.

## 2D- und 3D-Darstellung

[Prüfen Sie Pläne und räumliche Beziehungen](views.md), wählen Sie Darstellungsstile und nutzen Sie Bewegungs-, Achsen- und Einrastfunktionen. Passen Sie Modelldetails für größere Szenen an.

## Szenenbearbeitung, Tabellen, Patch und Gruppen

Bearbeiten Sie Scheinwerfer, Traversen, Kettenzüge und Objekte; prüfen Sie Metadaten und Patch; organisieren Sie Ebenen und Gruppen. Nutzen Sie Tabellenbearbeitung, fortlaufende Platzierung, Undo/Redo und die integrierte [Console](shortcuts-and-command-bar.md).

## Aus Text erstellen

[Erzeugen Sie Ausgangsszenen](create-from-text.md) aus eingefügtem Text, TXT- oder PDF-Ridern, prüfen Sie Typzuordnungen, wählen Sie Modi oder verwenden Sie generische Ersatzprofile.

## Layouts und PDF-Dokumentation

[Ordnen Sie Druckseiten an](layouts.md) mit 2D-Ansichten, Legenden, Veranstaltungstabellen, Text und Bildern. Teilen Sie Vorlagen und exportieren Sie Layout-PDFs.

## Experimentelle lokale Integration

OSC und [MCP](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/mcp_adapter.md) bieten experimentelle lokale Integration für separat konfigurierte Automatisierung. Technische Verträge stehen in der [Entwicklerdokumentation](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/index.md).

## Plattformen und Quellcode

Perastage unterstützt Windows, macOS und Linux und steht unter GNU GPL Version 3 oder höher. Siehe [Installation](installation.md) für Pakete und das [Repository](https://github.com/PeramatoG/Perastage) für Quellcode und Beiträge.

## Manuelles DMX-Patchen

Die manuelle Eingabe für ein Gerät erhält einen gültigen Universe/Channel-Start (Kanal 1–512), auch wenn der primäre GDTF-Kanalumfang das Universum überschreitet. Eine rote Channel-Zelle erläutert beim Darüberfahren Überlauf, unbekannten Kanalumfang oder Überschneidung. 180 Kanäle ab 1.400 würden bei hypothetischem kontinuierlichem Auto Span die Bereiche 1.400–1.512 und 2.1–2.67 benötigen. Dies ist nur eine Information: Standard-MVR speichert den Start und garantiert dieses Routing nicht; Empfänger oder Controller müssen es unterstützen. Sekundäre DMX-Breaks bleiben erhalten. AutoPatch und sequenzielles Patchen mehrerer Geräte behalten ihre bisherigen Universumswechsel bei.

## DMX-Patch-Ansicht

Öffnen Sie **Ansicht → DMX Patch**, um die 512 Kanäle eines Universums unter der aktiven 2D-/3D-Ansicht zu prüfen. Kompakte Zellen und die Spaltenanzahl passen sich automatisch an die verfügbare Breite an; geben Sie eine Universumsnummer ein, um ein leeres Universum anzuzeigen. Die Farben entsprechen den Gerätefarben, die Beschriftungen den physischen GDTF-Kanalattributen. Tooltips nennen alle belegenden Geräte und Breaks. Schraffierte Zellen zeigen Überschneidungen. Die Diagnoseliste erklärt fehlende Definitionen, Zuweisungen und Überläufe; es erfolgt keine automatische Fortsetzung im nächsten Universum. Die Ansicht ist schreibgeschützt. Beim Schließen wird Platz freigegeben; Sichtbarkeit und Höhe bleiben gespeichert. Im Layout- und Inspector-Modus wird sie vorübergehend ausgeblendet. Verwenden Sie die Titelleisten-Schaltflächen zum Schließen oder Maximieren. Das Warnsymbol öffnet eine scrollbare Diagnoseliste statt eines langen Tooltips.

Jeder Gerätename erscheint nur in der ersten Zeile; zentrierte Kanalnummern, ein eingerahmter erster Kanal und abgerundete Ecken markieren die Grenzen. **Deklarierte Offsets** ist für mehrdeutige Definitionen standardmäßig aktiviert: Vorläufige Bytes werden relativ zur primären Startadresse mit gestrichelten Rändern, Fragezeichen und einem separaten Zähler für bestätigte Belegung und Konflikte angezeigt. Die Option korrigiert keine Breaks, schreibt keine Dateien um und setzt keine Kanäle im nächsten Universum fort. Auswahl- und Hover-Umrisse bleiben mit Tabelle und Ansichten synchron: Cyan für die Auswahl und die grünen Hover-Farben der Tabelle, ohne die Füllfarben zu ändern. Gerätenamen erscheinen fett. Ein Klick auf einen Kanal wählt seine Geräte in der Tabelle und beiden Ansichten aus; Strg fügt hinzu und Umschalt schaltet die Auswahl um. Ein einfacher Klick auf einen freien Kanal hebt die Auswahl auf. Gruppeneinstellungen werden berücksichtigt; überlappende Kanäle wählen alle belegenden Geräte. Die Auswahl ändert keine DMX-Adressen.
