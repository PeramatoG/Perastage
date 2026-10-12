# Tastenkürzel und Befehlsleiste

Diese Referenz bündelt praktische Tastatur-, Maus- und Befehlsabläufe aus der integrierten Hilfe, damit Sie schneller arbeiten können, ohne die Dokumentation zu verlassen.

## Szenenbearbeitung

- `Ctrl+X`: Ausgewählte Scheinwerfer, Traversen, Kettenzüge und Szenenobjekte ausschneiden.
- `Ctrl+C`: Ausgewählte Szenenelemente kopieren.
- `Ctrl+V`: Kopierte Elemente in das aktuelle Projekt einfügen.

Diese Befehle unterstützen gemischte Auswahlen und stehen im Menü **Edit** bereit. Beim Bearbeiten eines Textfelds oder einer Tabellenzelle behalten sie ihre normale Textbearbeitungsfunktion.

## Globale Tastenkürzel

| Kürzel | Aktion |
| --- | --- |
| `Ctrl+N` | Neues Projekt |
| `Ctrl+L` | Projekt laden |
| `Ctrl+S` | Projekt speichern |
| `Ctrl+Q` | Anwendung schließen |
| `Ctrl+Z / Ctrl+Y` | Rückgängig / Wiederholen |
| `Del` | Auswahl löschen |
| `Ctrl+G` | Aktive tabellenübergreifende Auswahl gruppieren |
| `Ctrl+U` | Ausgewählte Objekte aus ihrer direkten Gruppe lösen |
| `F1` | Hilfe öffnen |
| `Ctrl+Alt+D` / `Ctrl+Shift+D` | Scheinwerfer auf Traverse / zwischen Punkten verteilen |
| `P` / `R` | Console fokussieren und `pos ` / `rot ` außerhalb bearbeitbarer Felder vorfüllen |
| `F` | Console fokussieren und `fixture ` außerhalb bearbeitbarer Felder vorfüllen |
| `1 / 2 / 3 / 4` | Zu Fixtures / Trusses / Hoists / Objects wechseln |

## Ansichtskürzel

| Kürzel | Aktion |
| --- | --- |
| `Z` | Fokussierte 2D-/3D-Ansicht einpassen |
| `NumPad1` | Vorderansicht |
| `NumPad3` | Seiten-/Rechtsansicht |
| `NumPad7` | Draufsicht |
| `NumPad5` | Ausrichtung der 3D-Kamera zurücksetzen |
| `Arrow keys` (3D) | Kamera orbitieren |
| `Shift + Arrow keys` (3D) | Kamera verschieben |
| `Alt + Arrow keys` (2D/3D) | Ein-/auszoomen |
| `Arrow keys` (2D) | Ansicht verschieben |

## Layoutansichtskürzel

| Kürzel | Aktion |
| --- | --- |
| `Delete` | Ausgewähltes Layoutelement löschen |
| `Z` | Layoutansicht einpassen |

## Console-Eingabekürzel

| Kürzel | Aktion |
| --- | --- |
| `Esc` | Eingabe verlassen und Anwendungskürzel wieder aktivieren |
| `Up / Down` | Befehlsverlauf durchsuchen |
| `Home` | Zum Eingabeanfang nach dem Prompt springen |
| `Left / Backspace` | Bewegung vor den Prompt ist nicht möglich |

## Mauskürzel in der 3D-Ansicht

| Aktion | Ergebnis |
| --- | --- |
| Mit linker Maustaste ziehen | Kamera orbitieren |
| Umschalt + links ziehen oder mittlere Maustaste ziehen | Kamera verschieben |
| Mausrad | Ein-/auszoomen |
| Linksklick | Scheinwerfer/Traverse/Objekt unter dem Zeiger auswählen |
| Umschalt/Strg + Linksklick | Auswahl umschalten |
| Strg + links ziehen | Rechteckauswahl |
| Doppelklick auf Scheinwerferbeschriftung | Patch-Dialog öffnen |

## Schneller Ablauf mit der Befehlsleiste

1. Fokussieren Sie die Befehlsleiste.
2. Nutzen Sie außerhalb bearbeitbarer Felder `P`, `R` oder `F`, um die Console zu fokussieren und vorzufüllen.
3. Bearbeiten Sie den Befehlstext.
4. Führen Sie ihn aus und prüfen Sie Tabellen und Ansichten.
5. Verwenden Sie den Verlauf für wiederholte Änderungen.

## Experimentelle CLI

Ab 1.7.0 enthält Perastage die experimentelle `perastage-cli` für Automatisierung und fortgeschrittene Abläufe. Starten Sie sie im Terminal:

| Plattform | Beispiel |
| --- | --- |
| Windows (PowerShell, Standardinstallation) | `& "C:\Program Files\Perastage\perastage-cli.exe" --help` |
| macOS (App in Applications installiert) | `/Applications/Perastage.app/Contents/MacOS/perastage-cli --help` |
| Linux AppImage (Dateiname durch Ihren Download ersetzen) | `./Perastage-1.7.0-x86_64.AppImage --cli --help` |
| Arch Linux | `perastage-cli --help` |

Prüfen Sie die installierte Version mit `--version`. Windows und macOS nehmen die CLI nicht in PATH auf. Befehle und Optionen bleiben Englisch. `inspect` liest Dateien ohne Änderungen; `scene` benötigt eine separate Ausgabedatei und überschreibt seine Quelle nie; `live` verbindet nur über Loopback mit einer laufenden lokalen Perastage-Instanz. Experimentell bezeichnet den Reifegrad von Unterstützung und Stabilität; Validierungs- und Atomizitätsregeln gelten weiterhin. Der MCP-Adapter wird separat konfiguriert und ist nicht im Installer enthalten. Details zur Grammatik finden Sie unter [Developer CLI](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/cli.md).

<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->

## Häufige und nützliche Console-Befehle

### Auswahlbefehle

| Befehl | Beschreibung |
| --- | --- |
| `clear` | Scheinwerfer-, Traversen- und Objektauswahl aufheben |
| `f ...` | Scheinwerfer nach ID auswählen |
| `t ...` | Traversen nach Einheitennummer auswählen; vorher Traversenauswahl aufheben |

Syntaxbeispiele:

- Einzelne ID: `f 12`
- Bereich: `f 1-5`, `f 1 thru 5`, `f 1 t 5`
- Hinzufügen/entfernen: `f + 10 - 3`
- Gemischt: `f 1 3 5 7-9`

### Positions- und Rotationsbefehle

| Befehl | Beschreibung |
| --- | --- |
| `pos x <values>` | X-Positionen setzen |
| `pos y <values>` | Y-Positionen setzen |
| `pos z <values>` | Z-Positionen setzen |
| `pos <x>,<y>,<z>` | X/Y/Z gemeinsam setzen |
| `x <values>` / `y <values>` / `z <values>` | Achsenkürzel für `pos` |
| `rot x <values>` / `rot y <values>` / `rot z <values>` | Roll/Pitch/Yaw setzen |
| `rot x\|y\|z <values> --group` oder `--g` | Gesamte Auswahl als eine Gruppe drehen |

Hinweise:

- Console-Positionen und Pivotkoordinaten verwenden Meter; Rotationen verwenden Grad.
- Ein Wert gilt für alle ausgewählten Elemente.
- Zwei Werte verteilen linear vom Anfang zum Ende der Auswahl; `t` und `thru` sind optionale Bereichstrenner. `pos x -7 t 7`, `pos x -7 thru 7` und `pos x -7 7` sind gleichwertig.
- Verwenden Sie `++` / `--` für relative Versätze. Kompakte und getrennte Werte werden akzeptiert, etwa `pos x ++1.5`, `pos x ++ 1.5`, `pos z --1` und `pos z -- 1`.
- Gruppenbewegung folgt den [Einstellungen für Auswahl und Bewegung](preferences.md#auswahl-und-bewegung).
- Der Pivot für Gruppenrotation ist standardmäßig die Mitte der Auswahl-Begrenzungsbox.
- Ein abschließendes Tripel `x,y,z` überschreibt den Pivot, etwa `rot y ++45 --g -2.5,0,0`.

### Transformationsräume

Weltachsen sind die Vorgabe für Befehlsleistentransformationen. Relative Positions- und Rotationsbefehle verwenden mit `--local` oder `-l` lokale Achsen:

```text
f 1
pos x ++ 1
pos x ++ 1 --local
rot y ++ 45
rot y ++ 45 -l
rot z ++ 30 --group --local
```

Raummodifikatoren betreffen inkrementelle Vorgänge (`++` und `--`). Absolute Positionen setzen weiterhin Weltkoordinaten; absolute Rotationen behalten das bisherige Euler-Verhalten. Die Schaltfläche **Local Axes** bleibt gesetzt und betrifft nur Ansichtstransformationen. Verlauf und Skripte bleiben deterministisch und übernehmen diesen Zustand nicht.

<!-- PERASTAGE_CONSOLE_HELP_END -->

## Fokus und Priorität

- Kürzel sind während der Eingabe in bearbeitbare Felder oder Zellen gesperrt.
- Ansichtskürzel wirken in der fokussierten Ansicht.
- Dialogkürzel bleiben auf ihren Dialog beschränkt.

## Lokale Kürzel im Dialog zum Erstellen aus Text

| Kürzel | Aktion |
| --- | --- |
| `↑` / `↓` | Vorschläge durchlaufen |
| `Enter` oder `Tab` | Ausgewählten Vorschlag übernehmen |
| `Esc` | Vorschläge schließen, ohne den Dialog zu schließen |

Seitenbearbeitung und Layout-Mausbedienung beschreibt [Layouts und PDF](layouts.md).
