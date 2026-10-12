# Einstellungen

Öffnen Sie **Edit → Preferences**. Ein Kategorienwechsel behält ausstehende Änderungen. **Apply** speichert ohne Schließen, **OK** speichert und schließt, **Cancel** verwirft Änderungen seit dem letzten Apply.

## Allgemein

- **Language:** Wählen Sie die Oberflächensprache und starten Sie Perastage neu.
- **Units:** Wählen Sie Längen- und Gewichtseinheiten. Änderungen gelten sofort.
- **Updates:** **Check on startup (recommended)** prüft höchstens alle 24 Stunden. Erinnerungen für einzelne Versionen lassen sich unterdrücken. **Manual only** deaktiviert Startprüfungen; **Help → Check for Updates** prüft weiterhin auf Anfrage.

### Cache

**General → Cache** zeigt den schreibgeschützten Cache-Pfad und die aktuelle Speicherbelegung. **Open Cache Folder** öffnet den Ordner im Dateimanager. **Clear Cache...** fragt nach Bestätigung und löscht regenerierbare Perastage-Caches sofort, unabhängig von Apply, OK oder Cancel. Geometrie wird bei Bedarf neu erstellt; der GDTF-Katalog wird online erneut heruntergeladen. Konfiguration, Protokolle, Zugangsdaten, Bibliotheken, GDTFs, Verzeichnisse und Projekte bleiben erhalten. Können Dateien nicht gelöscht werden, meldet Perastage den Fehler und aktualisiert die angezeigte Größe. Dauerhafte Caches liegen in `cache/` unter den Anwendungsdaten des Benutzers statt neben Quellressourcen.

## Import

**Rider Import** steuert **Auto patch after import**, automatische Ebenen nach Position oder Scheinwerfertyp sowie LX-Standardhöhen, -positionen und -ränder. Längenfelder verwenden Ihre Einheiten. Siehe [Aus Text erstellen](create-from-text.md).

## Ansicht

### 3D-Ansicht

Wählen Sie die Darstellung und **Performance → Model detail**:

- **Standard** (Vorgabe) bietet einen Ausgleich zwischen Detail und Leistung.
- **Low** vereinfacht große Modelle stärker.
- **High** zeigt alle verfügbaren Originaldetails.

**Use a simplified proxy while navigating** ist unabhängig und standardmäßig deaktiviert. Es vereinfacht nicht ausgewählte Scheinwerfer während schneller Navigation vorübergehend und stellt danach das gewählte Detail wieder her. Änderungen gelten beim nächsten Zeichnen ohne erneutes Öffnen. Quellgeometrie, GDTF-Ressourcen, 2D-Symbole und exportierte Geometrie bleiben unverändert.

### Auswahl und Bewegung

Wählen Sie, welche Objekttypen bei Maus-, Console- und Magnet-Transformationen ihre übergeordnete Wurzelgruppe verschieben. Gruppierte Traversen bewegen standardmäßig ihre Gruppe; Scheinwerfer, Stützen/Kettenzüge und Szenenobjekte bewegen sich unabhängig. Direkte Gruppenauswahl bewegt die Gruppe. Tabellenänderungen betreffen nur das bearbeitete Objekt.

**Show anchor references while moving or inserting elements** zeigt kompatible Magnet-Anker beim Verschieben oder Einfügen. Die Hilfslinien gehören weder zur Szene noch zum Export. Änderungen der Bewegungseinstellungen ändern weder die Projekthierarchie noch den Bearbeitungsstatus.

## Formate

### GDTF-Ressourcen

GDTF-Einstellungen verwalten Share-Zugangsdaten. Perastage speichert scheinwerferweite Top/Front/Side/Bottom-Symbole automatisch in PSTG. Speichern Sie das Projekt, um erzeugte Symbole und Benutzerüberschreibungen zu behalten; diese werden keine GDTF- oder MVR-Ressourcen. Der Fixture Editor prüft ursprüngliche SVGs pro Model und erhält ihre Bytes und Versätze. Ausdrückliche Änderungen physikalischer Eigenschaften bleiben verfügbar.

### MVR-Import / -Export

**Truss geometry export mode** bietet:

- **Standard MVR representation** (Vorgabe): Importierte Symbolverweise möglichst erhalten.
- **Direct Geometry3D for truss symbols:** Symbolverweise für empfangende Anwendungen mit Bedarf an direkter Geometrie auflösen.

Beide schreiben MVR-1.6-kompatible Ausgaben. Projektspeicherung behält die Standarddarstellung. Importoptionen beschreibt [MVR-Dateien öffnen](opening-mvr-files.md).
